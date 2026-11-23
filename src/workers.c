/* Workers (spec 047): processes that run beside the program and exchange messages with it.
 * A worker is a fork of the program, as a concurrent_map task is (src/task.c), that is not waited for.
 * Two pipes each: the inbox (program to worker: a length and a message) and the control pipe (worker to
 * program: frames of a kind byte, a length and a payload, as task_send writes them). The worker's
 * output travels as frames too, in order with its messages, and the program writes it when it reads
 * past it: the same bytes on every run (research R10). A recording holds the frames each operation
 * passed, and a replay runs the operations over them without starting a process (R11).
 * ponytail: workers are started by the program's own process only; a table per process if a task or a
 * serving worker ever needs one (R7).
 * ponytail: receive_first looks through each worker's unread output for its next message every time
 * it is called; an index of the next message per slot if a worker prints megabytes between messages. */
#include "vo.h"

#define W_MAX 64
#define PIPE_BYTES (1L << 20)
#define MSG_MAX (16L << 20)  /* one message, encoded */
#define TAKE_MAX (64L << 20) /* taken in from a worker while the program waits to send to it */
#define POLLIN 1
#define POLLOUT 4
#define SIGPIPE 13
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define F_SETFL 4

/* Frames on the control pipe. 4 is task.c's TM_TRACE: the trace buffer's fd -4 sends it. */
enum { WM_TRACE = 4, WM_MSG = 10, WM_OUT, WM_ERR, WM_END, WM_FAIL, WM_TIMEOUT /* in recordings only */ };
enum { WO_START = 1, WO_TELL, WO_RECEIVE, WO_FIRST, WO_STOP, WO_NEXT, WO_EMIT };
enum { WE_TIMEOUT, WE_ENDED, WE_FAILED, WE_FULL, WE_TOO_LARGE, WE_TOO_MANY }; /* WorkerError's cases, std/workers.vo */
enum { GOT_NONE, GOT_MSG, GOT_END, GOT_FAIL, GOT_TIMEOUT };

typedef struct WSlot {
    long pid;    /* 0: free; -1: in a replay (no process) */
    long gen;    /* a handle is gen * 64 + the slot's index: one of an ended worker names no later one */
    long number; /* among the workers started, from 0 */
    int in, ctl; /* the inbox's write end; the control pipe's read end (-1 at its end) */
    Type *tin, *tout;
    SB *buf; /* frames that arrived and were not passed yet, from `at` (globals are collector roots) */
    long at;
} WSlot;

typedef struct PollFd {
    int fd;
    short events, revents;
} PollFd;

long worker_self; /* 1 in a worker's own process */
static WSlot ws[W_MAX];
static long w_started;
static int self_in = -1, self_ctl = -1;
static Type *self_tin, *self_tout;
static SB *w_rec;      /* recording: the frames the operation under way passed */
static int w_rec_msgs; /* ... messages among them (not when they are dropped) */
static long w_fail_code, w_fail_node; /* the first failure w_close met: for a test's report */
static Str *w_fail_msg;

static void w_internal(char *what) NORETURN;
static void w_internal(char *what)
{
    out_flush();
    toolchain_error((int)opt_json, 2, "T0003", str_from_cstr(what), 0);
    vo_exit(EXIT_INTERNAL);
    for (;;) {}
}

/* ---- results ---- */

static Variant *w_err(long tag, Str *text)
{
    GC_ROOT(&text);
    Variant *e = pg_new_variant(tag, (long)text);
    GC_ROOT(&e);
    Variant *r = make_result(1, (long)e);
    GC_UNROOT(2);
    return r;
}

/* Milliseconds of a `Wait`: -1 for `forever`. */
static long w_wait_ms(Variant *wait, Node *call)
{
    if (wait->tag == 0) return -1;
    if (wait->pay.i < 0)
        rt_raise("R0007", msg("The time limit $1 is negative.", int_to_str(wait->pay.i), 0, 0), call);
    return wait->pay.i;
}

/* The message types of the operation's caller: the wrappers of std/workers.vo are generic in In and
 * Out, in that order (R2). */
static void w_types(Type **in, Type **out)
{
    Vec *targs = ((Frame *)cur_frame)->targs;
    *in = targs->data[0].t;
    *out = targs->data[1].t;
}

/* The slot a handle names, or 0 when its worker has ended. A handle used with other message types
 * would be decoded wrongly: R0007. */
static WSlot *w_slot(long id, Node *call)
{
    WSlot *s = &ws[id & (W_MAX - 1)];
    if (id < 0 || !s->pid || s->gen != id >> 6) return 0;
    Type *in, *out;
    w_types(&in, &out);
    if (s->tin != in || s->tout != out)
        rt_raise_c("R0007", "This handle was not made by `start_worker` for these message types.", call);
    return s;
}

/* ---- frames ---- */

static void put_word(SB *sb, long w)
{
    sb_bytes(sb, (char *)&w, 8);
}

static void w_rec_frame(int kind, char *p, long n)
{
    if (rcd_mode != 1) return;
    if (!w_rec) w_rec = sb_new();
    sb_byte(w_rec, (char)kind);
    put_word(w_rec, n);
    sb_bytes(w_rec, p, n);
}

/* Recording: the operation's event, with the frames it passed. */
static void w_rec_event(char *name)
{
    if (rcd_mode != 1) return;
    if (!w_rec) w_rec = sb_new();
    Str *v = str_new(w_rec->ptr, w_rec->len);
    GC_ROOT(&v);
    rcd_event(EVK_WORKER, str_from_cstr(name), -1, v, 0);
    GC_UNROOT(1);
    w_rec = 0;
}

/* Replay: the slot's frames are those of the event (after `skip` bytes). */
static void w_load(WSlot *s, Event *e, long skip)
{
    Str *v = e->value.s;
    s->buf = sb_new();
    s->at = 0;
    if (v && v->len > skip) sb_bytes(s->buf, v->data + skip, v->len - skip);
}

/* The frame at `at`: its kind, with its payload in *p and *n; 0 when it has not all arrived. */
static int w_frame(WSlot *s, long at, char **p, long *n)
{
    SB *b = s->buf;
    if (b->len - at < 9) return 0;
    long len;
    mem_copy((char *)&len, b->ptr + at + 1, 8);
    if (len < 0 || len > b->len - at - 9) return 0;
    *p = b->ptr + at + 9;
    *n = len;
    return (unsigned char)b->ptr[at];
}

/* Reads what the control pipe has into the slot; at the pipe's end it is closed. */
static void w_fill(WSlot *s)
{
    SB *b = s->buf;
    if (s->at == b->len) {
        b->len = s->at = 0;
    } else if (s->at > PIPE_BYTES) { /* what was passed makes room */
        for (long i = s->at; i < b->len; i++) b->ptr[i - s->at] = b->ptr[i];
        b->len -= s->at;
        s->at = 0;
    }
    char buf[65536];
    long n = sys_read(s->ctl, buf, sizeof buf);
    if (n == -4 || n == -11) return; /* EINTR, EAGAIN */
    if (n <= 0) {
        sys_close(s->ctl);
        s->ctl = -1;
        return;
    }
    sb_bytes(b, buf, n);
}

/* Passes the frames at s->at up to and including the next message, end or failure: output is written
 * here, which is what puts it in order with the messages; trace records are taken. *p and *n: the
 * payload of the frame that stopped it (valid until the slot is filled again). */
static int w_pass(WSlot *s, char **p, long *n)
{
    for (;;) {
        int kind = w_frame(s, s->at, p, n);
        if (!kind) return GOT_NONE;
        s->at += 9 + *n;
        if (kind != WM_TRACE && (kind != WM_MSG || w_rec_msgs)) {
            w_rec_frame(kind, *p, *n);
            if (kind == WM_FAIL && rcd_mode == 1 && *n >= 16) /* addresses mean nothing in a recording */
                for (long i = 0; i < 16; i++) w_rec->ptr[w_rec->len - *n + i] = 0;
        }
        switch (kind) {
        case WM_OUT:
            if (cap_on) cap_bytes(*p, *n); /* a test's workers' output stays with the test */
            else out_bytes(1, *p, *n);
            break;
        case WM_ERR: out_bytes(2, *p, *n); break;
        case WM_TRACE: tr_worker_take(s->number, *p, *n); break;
        case WM_MSG: return GOT_MSG;
        case WM_END: return GOT_END;
        case WM_FAIL: return GOT_FAIL;
        case WM_TIMEOUT: return GOT_TIMEOUT;
        default: w_internal("a worker sent an unknown message");
        }
    }
}

/* 1 when a message, an end or a failure of the worker has arrived (or it is gone without one). */
static int w_ready(WSlot *s)
{
    long at = s->at;
    for (;;) {
        char *p;
        long n;
        int kind = w_frame(s, at, &p, &n);
        if (!kind) return s->ctl < 0;
        if (kind == WM_MSG || kind == WM_END || kind == WM_FAIL) return 1;
        at += 9 + n;
    }
}

/* A failure frame's parts: code and node (the program's own addresses, 0 for an exit or a signal), the
 * message, then the report as the program would have printed it. */
static Str *fail_msg(char *p, long n)
{
    long len = 0;
    if (n >= 24) mem_copy((char *)&len, p + 16, 8);
    if (len < 0 || len > n - 24) len = 0;
    return str_new(p + 24, len);
}

/* What `failed` carries: the report without its last newline, or the message when there is none. */
static Str *fail_text(char *p, long n)
{
    Str *m = fail_msg(p, n);
    long at = 24 + m->len;
    if (at >= n) return m;
    long len = n - at;
    if (p[n - 1] == '\n') len--;
    return str_new(p + at, len);
}

static void fail_frame(SB *sb, char *code, Node *node, char *m, long mn, char *report, long n)
{
    put_word(sb, (long)code);
    put_word(sb, (long)node);
    put_word(sb, mn);
    sb_bytes(sb, m, mn);
    sb_bytes(sb, report, n);
}

/* The slot is free again: its process (if it still runs) is ended and gone when this returns. */
static void w_free(WSlot *s)
{
    if (s->pid > 0) {
        int st;
        sys_kill(s->pid);
        sys_wait(s->pid, &st);
    }
    if (s->in >= 0) sys_close(s->in);
    if (s->ctl >= 0) sys_close(s->ctl);
    s->in = s->ctl = -1;
    s->pid = 0;
    s->gen++;
    s->buf = 0;
}

/* Waits until one of the slots' control pipes has something (read into its slot) or `until` passes
 * (-1: never): 0 on that timeout. --timeout's deadline is R0009, as everywhere. */
static int w_wait(WSlot **slots, long count, long until, Node *call)
{
    PollFd p[W_MAX];
    WSlot *owner[W_MAX];
    long np = 0;
    for (long i = 0; i < count; i++)
        if (slots[i]->ctl >= 0) {
            p[np] = (PollFd){slots[i]->ctl, POLLIN, 0};
            owner[np++] = slots[i];
        }
    long now = sys_clock_ms(), wait = -1;
    if (until >= 0) {
        wait = until - now;
        if (wait <= 0) wait = 0;
    }
    if (deadline_ms && (wait < 0 || deadline_ms - now < wait)) {
        if (deadline_ms <= now) rt_timeout(call);
        wait = deadline_ms - now;
    }
    long ts[2] = {wait / 1000, wait % 1000 * 1000000};
    if (wait) out_flush(); /* what the program printed is seen before it waits */
    long r = vo_syscall5(SYS_ppoll, (long)p, np, wait < 0 ? 0 : (long)ts, 0, 8);
    if (r < 0 && r != -4) w_internal("cannot wait for a worker");
    for (long i = 0; i < np && r > 0; i++)
        if (p[i].revents) w_fill(owner[i]);
    return r != 0 || until < 0 || sys_clock_ms() < until;
}

/* ---- the program's side ---- */

/* The worker's next message as a result: what `receive` returns. Output before it is written. */
static Variant *w_take(WSlot *s, long wait_ms, Type *out, Node *call)
{
    long until = wait_ms < 0 ? -1 : sys_clock_ms() + wait_ms;
    for (;;) {
        char *p;
        long n;
        switch (w_pass(s, &p, &n)) {
        case GOT_MSG: {
            long v = task_decode(p, n, out);
            GC_ROOT(&v);
            Variant *r = make_result(0, v);
            GC_UNROOT(1);
            return r;
        }
        case GOT_END: w_free(s); return w_err(WE_ENDED, 0);
        case GOT_FAIL: {
            Str *text = fail_text(p, n);
            w_free(s);
            return w_err(WE_FAILED, text);
        }
        case GOT_TIMEOUT: return w_err(WE_TIMEOUT, 0);
        }
        if (rcd_mode == 2) rp_mismatch(EVK_WORKER, str_from_cstr("receive"), 0); /* a recording cut short */
        if (s->ctl < 0) { /* gone without a last word: ended from outside */
            int st = 0;
            sys_kill(s->pid);
            sys_wait(s->pid, &st);
            s->pid = -1;
            Str *text = (st & 127) ? msg("signal $1", int_to_str(st & 127), 0, 0)
                                   : msg("exit status $1", int_to_str((st >> 8) & 255), 0, 0);
            GC_ROOT(&text);
            if (rcd_mode == 1) {
                SB *sb = sb_new();
                GC_ROOT(&sb);
                fail_frame(sb, 0, 0, text->data, text->len, 0, 0);
                w_rec_frame(WM_FAIL, sb->ptr, sb->len);
                GC_UNROOT(1);
            }
            w_free(s);
            GC_UNROOT(1);
            return w_err(WE_FAILED, text);
        }
        if (!w_wait(&s, 1, until, call)) {
            w_rec_frame(WM_TIMEOUT, 0, 0);
            return w_err(WE_TIMEOUT, 0);
        }
    }
}

static Variant *w_receive(long id, Variant *wait, Node *call)
{
    long ms = w_wait_ms(wait, call);
    WSlot *s = w_slot(id, call);
    if (!s) return w_err(WE_ENDED, 0);
    if (rcd_mode == 2) w_load(s, rp_take(EVK_WORKER, str_from_cstr("receive")), 0);
    w_rec_msgs = 1;
    Variant *r = w_take(s, ms, s->tout, call);
    GC_ROOT(&r);
    w_rec_event("receive");
    GC_UNROOT(1);
    return r;
}

/* ok(Arrival { index, message }) */
static Variant *w_arrival(long index, Variant *message)
{
    GC_ROOT(&message);
    Record *a = alloc_obj(OBJ_RECORD, 16);
    a->fields[0].i = index;
    a->fields[1].p = message;
    GC_ROOT(&a);
    Variant *r = make_result(0, (long)a);
    GC_UNROOT(2);
    return r;
}

static Variant *w_first(List *handles, Variant *wait, Node *call)
{
    long ms = w_wait_ms(wait, call);
    long until = ms < 0 ? -1 : sys_clock_ms() + ms;
    GC_ROOT(&handles);
    Variant *r = 0;
    Event *e = 0;
    for (;;) {
        WSlot *live[W_MAX];
        long index[W_MAX], n = 0;
        for (long i = 0; i < handles->len && n < W_MAX; i++) {
            WSlot *s = w_slot(((Record *)handles->data[i].p)->fields[0].i, call);
            if (!s) continue; /* ended, and said so once */
            long seen = 0;
            while (seen < n && live[seen] != s) seen++;
            if (seen < n) continue; /* the same worker twice in the list */
            live[n] = s;
            index[n++] = i;
        }
        if (!n) { /* every worker of the list has been reported */
            r = w_err(WE_ENDED, 0);
            break;
        }
        long pick = -1;
        if (rcd_mode == 2) {
            e = rp_take(EVK_WORKER, str_from_cstr("first"));
            long at = -1;
            if (e->value.s && e->value.s->len >= 8) mem_copy((char *)&at, e->value.s->data, 8);
            for (long k = 0; k < n; k++)
                if (index[k] == at) pick = k;
            if (pick < 0 && at >= 0) rp_mismatch_now(EVK_WORKER, str_from_cstr("first"));
            if (pick < 0) { /* the recorded run's limit passed */
                r = w_err(WE_TIMEOUT, 0);
                break;
            }
            w_load(live[pick], e, 8);
        } else {
            for (long k = 0; k < n && pick < 0; k++)
                if (w_ready(live[k])) pick = k; /* the earliest in the list (FR-013) */
        }
        if (pick >= 0) {
            if (rcd_mode == 1) {
                w_rec = sb_new();
                put_word(w_rec, index[pick]);
            }
            w_rec_msgs = 1;
            Variant *m = w_take(live[pick], 0, live[pick]->tout, call);
            GC_ROOT(&m);
            w_rec_event("first");
            r = w_arrival(index[pick], m);
            GC_UNROOT(1);
            break;
        }
        if (!w_wait(live, n, until, call)) {
            if (rcd_mode == 1) {
                w_rec = sb_new();
                put_word(w_rec, -1);
                w_rec_event("first");
            }
            r = w_err(WE_TIMEOUT, 0);
            break;
        }
    }
    GC_UNROOT(1);
    return r;
}

/* Ends the worker and passes what had arrived from it: its output is written, its messages are
 * dropped. 1 when a failure was among it; `report` writes that failure to standard error. In a replay
 * e is the recorded event (0: nothing was recorded). */
static int w_close(WSlot *s, char *name, int report, Event *e)
{
    if (rcd_mode == 2) {
        s->buf = sb_new();
        s->at = 0;
        if (e) w_load(s, e, 0);
    } else if (s->pid > 0) {
        int st;
        sys_kill(s->pid);
        sys_wait(s->pid, &st);
        s->pid = -1;
        while (s->ctl >= 0) w_fill(s); /* (its own tasks, which hold the pipe too, end with it) */
    }
    int failed = 0;
    w_rec_msgs = 0;
    for (;;) {
        char *p;
        long n;
        int got = w_pass(s, &p, &n);
        if (got == GOT_NONE) break;
        if (got != GOT_FAIL) continue;
        failed = 1;
        long code = 0, node = 0;
        if (n >= 16) {
            mem_copy((char *)&code, p, 8);
            mem_copy((char *)&node, p + 8, 8);
        }
        if (!w_fail_msg) {
            w_fail_code = code;
            w_fail_node = node;
            w_fail_msg = fail_msg(p, n);
        }
        if (!report) continue;
        Str *m = fail_msg(p, n);
        if (24 + m->len < n) {
            out_bytes(2, p + 24 + m->len, n - 24 - m->len); /* the runtime error, as the worker reported it */
        } else {
            out_cstr(2, "error: a worker ended with ");
            out_str(2, m);
            out_cstr(2, "\n");
        }
    }
    w_rec_event(name);
    w_free(s);
    return failed;
}

/* Holds SIGPIPE back around a write to a worker that may be gone (on: before; off: after, dropping one
 * that is pending), as std.process does for a child that stopped reading. */
static void w_sigpipe(int on, unsigned long *old)
{
    unsigned long sig = 1UL << (SIGPIPE - 1);
    if (on) {
        vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&sig, (long)old, 8);
        return;
    }
    int fd = (int)vo_syscall4(SYS_signalfd4, -1, (long)&sig, 8, O_NONBLOCK | O_CLOEXEC);
    char info[128];
    while (fd >= 0 && sys_read(fd, info, sizeof info) > 0) {}
    if (fd >= 0) sys_close(fd);
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)old, 0, 8);
}

static Variant *w_tell(long id, long value, Node *call)
{
    GC_ROOT(&value);
    WSlot *s = w_slot(id, call);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    Variant *r = 0;
    if (!s) { /* its end was received (or it was stopped): the one case `tell` reports (R4) */
        r = w_err(WE_ENDED, 0);
        goto done;
    }
    put_word(sb, 0);
    task_encode(sb, value, s->tin, call);
    long len = sb->len - 8;
    if (len > MSG_MAX) {
        r = w_err(WE_TOO_LARGE, 0);
        goto done;
    }
    mem_copy(sb->ptr, (char *)&len, 8);
    int full = 0;
    if (rcd_mode == 2) {
        Event *e = rp_peek(EVK_WORKER, str_from_cstr("full"));
        if (e) {
            w_close(s, "full", 0, e);
            r = w_err(WE_FULL, 0);
        }
        goto done;
    }
    unsigned long old = 0;
    w_sigpipe(1, &old);
    for (long sent = 0; sent < sb->len;) {
        long w = sys_write(s->in, sb->ptr + sent, sb->len - sent);
        if (w > 0) {
            sent += w;
            continue;
        }
        if (w == -4) continue;
        if (w != -11) break; /* the worker is gone: the message is dropped, and `receive` will say why */
        /* The inbox is full. While waiting for room, take in what the worker sends, so that it is never
           waiting for the program to read while the program waits for it (FR-011). */
        PollFd p[2] = {{s->in, POLLOUT, 0}, {s->ctl, POLLIN, 0}};
        long wait = -1;
        if (deadline_ms) {
            wait = deadline_ms - sys_clock_ms();
            if (wait <= 0) {
                w_sigpipe(0, &old);
                rt_timeout(call);
            }
        }
        long ts[2] = {wait / 1000, wait % 1000 * 1000000};
        vo_syscall5(SYS_ppoll, (long)p, s->ctl >= 0 ? 2 : 1, wait < 0 ? 0 : (long)ts, 0, 8);
        if (s->ctl >= 0 && p[1].revents) w_fill(s);
        if (s->buf->len - s->at > TAKE_MAX) {
            full = 1; /* part of the message may be in the pipe: the worker cannot go on */
            break;
        }
    }
    w_sigpipe(0, &old);
    if (full) {
        w_close(s, "full", 0, 0);
        r = w_err(WE_FULL, 0);
    }
done:
    if (!r) r = make_result(0, 0);
    GC_UNROOT(2);
    return r;
}

static void w_stop(long id, Node *call)
{
    WSlot *s = w_slot(id, call);
    if (!s) return;
    w_close(s, "stop", 0, rcd_mode == 2 ? rp_take(EVK_WORKER, str_from_cstr("stop")) : 0);
    w_fail_msg = 0;
}

/* The program ends (vo_exit): every worker is ended, the output that had arrived is written, and a
 * failure nobody received is reported, with the status of a runtime error if the program's was 0. */
long workers_shutdown(long status)
{
    static int busy;
    if (busy) return status;
    busy = 1;
    for (int i = 0; i < W_MAX; i++) {
        WSlot *s = &ws[i];
        if (!s->pid) continue;
        Event *e = rcd_mode == 2 ? rp_peek(EVK_WORKER, str_from_cstr("exit")) : 0;
        if (w_close(s, "exit", 1, e) && status == 0) status = EXIT_RUNTIME;
    }
    busy = 0;
    return status;
}

/* A test ends: its workers with it. raise: a failure the test did not receive is the test's own
 * runtime error, at the place in the worker where it happened. */
void workers_end(long raise)
{
    w_fail_msg = 0;
    for (int i = 0; i < W_MAX; i++)
        if (ws[i].pid) w_close(&ws[i], "exit", 0, 0);
    w_started = 0;
    w_rec = 0;
    Str *m = w_fail_msg;
    w_fail_msg = 0;
    if (!raise || !m) return;
    GC_ROOT(&m); /* (rt_raise does not return: no unroot) */
    if (w_fail_code) rt_raise((char *)w_fail_code, m, (Node *)w_fail_node);
    rt_raise("R0007", msg("A worker ended with $1, and the test did not receive its failure.", m, 0, 0), (Node *)cur_node);
}

/* In a child of the program that is not one of its workers' own processes (a task, a serving worker):
 * the workers are the program's. Every handle reads as ended here (R6). */
void workers_forget(void)
{
    for (int i = 0; i < W_MAX; i++) {
        WSlot *s = &ws[i];
        if (!s->pid) continue;
        s->pid = -1; /* not this process's to end */
        w_free(s);
    }
    if (self_in >= 0) sys_close(self_in);
    if (self_ctl >= 0) sys_close(self_ctl);
    self_in = self_ctl = -1;
    worker_self = 0;
}

/* ---- the worker's side ---- */

/* In a worker: a piece of its output, as a frame (sys.c's out_flush). */
void worker_out(int fd, char *p, long n)
{
    if (n) task_send(fd == 1 ? WM_OUT : WM_ERR, p, n);
}

/* The worker's last frame, after its pending output and trace; the process ends. */
static void worker_final(int kind, char *p, long n) NORETURN;
static void worker_final(int kind, char *p, long n)
{
    deadline_ms = 0; /* nothing may raise any more */
    proc_sweep();
    out_flush();
    tr_task_end();
    task_send(kind, p, n);
    vo_syscall1(SYS_exit_group, 0);
    for (;;) {}
}

/* In a worker: a runtime error would end the program (interp.c). The program gets it as `failed`. */
void worker_fail(char *code, Node *node, Str *message, char *report, long n)
{
    SB *sb = sb_new();
    fail_frame(sb, code, node, message->data, message->len, report, n);
    worker_final(WM_FAIL, sb->ptr, sb->len);
}

/* In a worker: `exit` (vo_exit). 0 is its end; another status is a failure. */
void worker_exit(long status)
{
    if (!status) worker_final(WM_END, 0, 0);
    Str *m = msg("exit status $1", int_to_str(status), 0, 0);
    GC_ROOT(&m); /* (worker_final does not return: no unroot) */
    SB *sb = sb_new();
    fail_frame(sb, 0, 0, m->data, m->len, 0, 0);
    worker_final(WM_FAIL, sb->ptr, sb->len);
}

static void self_check(Node *call)
{
    Type *in, *out;
    w_types(&in, &out);
    if (!worker_self || in != self_tin || out != self_tout)
        rt_raise_c("R0007", "This link is not the one the worker's function was given.", call);
}

static int read_full(int fd, char *p, long n)
{
    while (n > 0) {
        long r = sys_read(fd, p, n);
        if (r == -4) continue;
        if (r <= 0) return 0;
        p += r;
        n -= r;
    }
    return 1;
}

static Variant *w_next(Variant *wait, Node *call)
{
    long ms = w_wait_ms(wait, call);
    self_check(call);
    out_flush(); /* what was printed so far goes up before the wait */
    tr_flush();
    if (!sys_wait_fd(self_in, POLLIN, ms)) return w_err(WE_TIMEOUT, 0); /* (--timeout is the program's) */
    long len = 0;
    /* (a worker never sees the program end: it is ended with it, and a closed inbox is the same) */
    if (!read_full(self_in, (char *)&len, 8) || len < 0 || len > MSG_MAX) vo_syscall1(SYS_exit_group, 0);
    Str *raw = alloc_obj(OBJ_STR, len + 9);
    raw->len = len;
    GC_ROOT(&raw);
    if (!read_full(self_in, raw->data, len)) vo_syscall1(SYS_exit_group, 0);
    long v = task_decode(raw->data, len, self_tin);
    GC_ROOT(&v);
    Variant *r = make_result(0, v);
    GC_UNROOT(2);
    return r;
}

static Variant *w_emit(long value, Node *call)
{
    self_check(call);
    GC_ROOT(&value);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    task_encode(sb, value, self_tout, call);
    Variant *r;
    if (sb->len > MSG_MAX) {
        r = w_err(WE_TOO_LARGE, 0);
    } else {
        out_flush(); /* what was printed before the message is before it on the pipe */
        tr_flush();
        task_send(WM_MSG, sb->ptr, sb->len);
        r = make_result(0, 0);
    }
    GC_UNROOT(2);
    return r;
}

/* ---- starting ---- */

static void worker_child(int *a, int *b, Closure *fn, Node *call, long parent, long number, Type *in, Type *out) NORETURN;
static void worker_child(int *a, int *b, Closure *fn, Node *call, long parent, long number, Type *in, Type *out)
{
    sys_die_with_parent(parent);
    workers_forget(); /* the other workers are the program's */
    sys_close(a[1]);
    sys_close(b[0]);
    self_in = a[0];
    self_ctl = b[1];
    task_ctl = self_ctl; /* task_send writes the frames */
    self_tin = in;
    self_tout = out;
    worker_self = 1;
    cap_on = 0;         /* its output goes up the pipe; the program captures it */
    recover_point = 0;  /* a runtime error here is the worker's own (interp.c hands it to worker_fail) */
    task_recover_base = 0;
    rcd_mode = 0;       /* a recording holds what the program got from the worker, not what the worker read */
    deadline_ms = 0;    /* --timeout is the program's: it ends the program, and the workers with it */
    rt_task_seed((number + 1) << 20); /* its own random sequence, by its number among the workers */
    tr_task_child();    /* its trace records go up as frames */
    Record *link = alloc_obj(OBJ_RECORD, 8);
    link->fields[0].i = 0;
    long arg = (long)link;
    call_value(fn, call, &arg);
    worker_final(WM_END, 0, 0);
}

/* ok(handle), or err(too_many). */
static Variant *w_start(Closure *fn, Node *call)
{
    Type *in, *out;
    w_types(&in, &out);
    if (task_depth || worker_self)
        rt_raise_c("R0007", "Workers are started by the program itself: not in a worker, a task or a serving worker.", call);
    Type *bad = ty_has_func(in) ? in : ty_has_func(out) ? out : 0;
    if (bad) rt_raise("R0015", msg("A worker's messages cannot hold a function: `$1`.", ty_name(bad), 0, 0), call);
    WSlot *s = 0;
    for (int i = 0; i < W_MAX && !s; i++)
        if (!ws[i].pid) s = &ws[i];
    if (!s) return w_err(WE_TOO_MANY, 0);
    GC_ROOT(&fn);
    if (!s->gen) s->gen = 1; /* (no handle is 0) */
    s->number = w_started++;
    s->tin = in;
    s->tout = out;
    s->buf = sb_new();
    s->at = 0;
    s->in = s->ctl = -1;
    if (rcd_mode == 2) { /* a replay runs no worker: its operations read the recording (R11) */
        s->pid = -1;
    } else {
        int a[2], b[2];
        if (sys_pipe(a) < 0 || sys_pipe(b) < 0) w_internal("cannot create a pipe for a worker");
        sys_pipe_size(a[1], PIPE_BYTES);
        sys_pipe_size(b[1], PIPE_BYTES);
        out_flush(); /* nothing pending is inherited and written twice */
        tr_flush();
        rcd_flush();
        long parent = sys_getpid();
        long pid = sys_fork();
        if (pid < 0) w_internal("cannot start a worker (the system refused to create a process)");
        if (pid == 0) worker_child(a, b, fn, call, parent, s->number, in, out);
        sys_close(a[0]);
        sys_close(b[1]);
        vo_syscall3(SYS_fcntl, a[1], F_SETFL, O_NONBLOCK | 1 /* O_WRONLY */);
        s->pid = pid;
        s->in = a[1];
        s->ctl = b[0];
    }
    GC_UNROOT(1);
    return make_result(0, s->gen * W_MAX + (s - ws));
}

/* __worker(OP, a, b) from std/workers.vo. */
long worker_op(Node *call, long a0, long a1, long a2)
{
    (void)a2;
    switch (((Node *)call->b.v->data[0].p)->a.i) {
    case WO_START: return (long)w_start((Closure *)a0, call);
    case WO_TELL: return (long)w_tell(a0, a1, call);
    case WO_RECEIVE: return (long)w_receive(a0, (Variant *)a1, call);
    case WO_FIRST: return (long)w_first((List *)a0, (Variant *)a1, call);
    case WO_STOP: w_stop(a0, call); return 0;
    case WO_NEXT: return (long)w_next((Variant *)a0, call);
    case WO_EMIT: return (long)w_emit(a0, call);
    }
    w_internal("unknown worker operation");
}
