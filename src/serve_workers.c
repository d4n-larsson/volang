/* serve_workers (spec 028): W worker processes, forked when the server starts, each run the setup once
 * and then answer one request at a time. The parent keeps the connections (serve.c's loop, R1): it gives
 * the oldest complete request to a free worker over that worker's request pipe and writes the response
 * the worker sends back on its control pipe, with the request's output (its stdout and stderr pipes)
 * written whole at that moment (R7). Messages on the control pipe use task.c's framing (task_send): the
 * task kinds for trace and recording events and the program's end, and the TW_ kinds below. */
#include "vo.h"

#define WORKER_MAX 64
#define PIPE_BYTES (1L << 20)
#define POLLIN 1
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define SIGPIPE 13
#define TM_TRACE 4 /* task.c's kinds */
#define TM_RECORD 5
#define TM_EXIT 3
#define TM_COUNTS 6

enum { TW_READY = 20, TW_SETUP_FAILED, TW_RESPONSE, TW_TAKE };
enum { W_NONE, W_STARTING, W_READY, W_BUSY };

typedef struct Worker {
    long pid;
    int req, ctl, out, err; /* the parent's ends: requests to it; messages, stdout, stderr from it (-1: closed) */
    int state;
    long conn;   /* the connection of the request it holds, -1 */
    long number; /* that request's arrival number */
    int pair;    /* spec 063: the parent's end of a socket pair: a taken connection goes down it (-1) */
    int talking; /* spec 063: its handler holds the request's connection (it stays W_BUSY) */
    SB *msgs, *obuf, *ebuf, *events; /* (globals are collector roots) */
} Worker;

static Worker sw[WORKER_MAX];
static long sw_n;
static Closure *sw_setup, *sw_answer; /* the functions the workers call */
static Node *sw_call;
static long sw_state; /* in a worker: the setup's result */
static long sw_answered; /* spec 055: responses relayed (the stop record's count of those drained) */
static int sw_pair = -1; /* spec 063: in a worker, its end of the socket pair (-1 elsewhere) */
static void worker_flush_events(void);

/* ---- spec 063 (research R1, R3): a handler in a worker takes its connection ---- */

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_CLOEXEC 0x80000
#define SOL_SOCKET 1
#define SCM_RIGHTS 1

typedef struct IoVec {
    void *base;
    long len;
} IoVec;

typedef struct MsgHdr {
    void *name;
    int namelen;
    int pad0;
    IoVec *iov;
    long iovlen;
    void *control;
    long controllen;
    int flags;
    int pad1;
} MsgHdr;

typedef struct FdControl { /* struct cmsghdr with one descriptor */
    long len;
    int level, type;
    int fd;
    int pad;
} FdControl;

int sw_in_worker(void)
{
    return sw_pair >= 0;
}

int sw_pair_fd(void)
{
    return sw_pair;
}

static void sw_gauge(void)
{
    long n = 0;
    for (long i = 0; i < sw_n; i++) n += sw[i].talking;
    sv_gauge(n);
}

/* At the first stop signal: every worker holding a conversation is woken (one byte, 9). */
static void sw_wake_talking(void)
{
    char stop = 9;
    for (long i = 0; i < sw_n; i++)
        if (sw[i].state == W_BUSY && sw[i].talking) vo_syscall6(SYS_sendto, sw[i].pair, (long)&stop, 1, 0x4000 | 0x40, 0, 0);
}

/* Sends one byte (1 granted, else the refusal: 2 busy, 3 bytes after the request) and, when granted, fd. */
static void sw_send_fd(int pair, char what, long fd)
{
    IoVec v = {&what, 1};
    FdControl c = {sizeof(long) + 2 * sizeof(int) + sizeof(int), SOL_SOCKET, SCM_RIGHTS, (int)fd, 0};
    MsgHdr m = {0, 0, 0, &v, 1, what == 1 ? &c : 0, what == 1 ? (long)sizeof c : 0, 0, 0};
    vo_syscall3(SYS_sendmsg, pair, (long)&m, 0x4000 /* MSG_NOSIGNAL */);
}

/* In a worker: asks the server for the connection of the request being handled; its descriptor, or -1
 * with *why. The server decides by the bound (research R3). */
long sw_take(long kind, long max_open, char **why)
{
    long msg_[2] = {kind, max_open};
    worker_flush_events();
    task_send(TW_TAKE, (char *)msg_, sizeof msg_);
    char what = 0;
    IoVec v = {&what, 1};
    FdControl c = {0};
    MsgHdr m = {0, 0, 0, &v, 1, &c, sizeof c, 0, 0};
    long n;
    do n = vo_syscall3(SYS_recvmsg, sw_pair, (long)&m, 0x40000000 /* MSG_CMSG_CLOEXEC */);
    while (n == -4);
    while (n == 1 && what == 9) { /* a stop byte meant for the handler before: not the answer */
        do n = vo_syscall3(SYS_recvmsg, sw_pair, (long)&m, 0x40000000);
        while (n == -4);
    }
    if (n == 1 && what == 1 && c.level == SOL_SOCKET && c.type == SCM_RIGHTS) return c.fd;
    *why = what == 2 ? "busy" : what == 3 ? "bytes after the request" : "no connection";
    return -1;
}

static Str *sw_500(void)
{
    return str_from_cstr("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
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

/* ---- the worker ---- */

/* Pending output and events go up before the message that ends a step (ready, a response). */
static void worker_flush(void)
{
    out_flush();
    tr_task_end();
    rcd_task_end();
}

static void worker_flush_events(void)
{
    worker_flush();
}

static void worker_run(Worker *w, int *p, long listener, long parent) NORETURN;
static void worker_run(Worker *w, int *p, long listener, long parent)
{
    sys_die_with_parent(parent);
    workers_forget(); /* spec 047: the program's workers are not this process's */
    sv_child_close(listener);
    for (int i = 0; i < WORKER_MAX; i++) { /* the other workers' pipes */
        Worker *o = &sw[i];
        if (o == w || o->state == W_NONE) continue;
        int *fds[5] = {&o->req, &o->ctl, &o->out, &o->err, &o->pair};
        for (int k = 0; k < 5; k++)
            if (*fds[k] >= 0) sys_close(*fds[k]);
    }
    sys_close(w->pair); /* the parent's end; p[8] is this worker's */
    sw_pair = p[8];
    /* p: request (0 read, 1 write), control, stdout, stderr (0 read, 1 write each) */
    sys_dup_to(p[5], 1);
    sys_dup_to(p[7], 2);
    for (int k = 0; k < 8; k++)
        if (k != 0 && k != 3) sys_close(p[k]);
    int req = p[0];
    task_ctl = p[3];
    cap_on = 0;
    task_depth++;
    task_top_depth = task_depth; /* a handler's tasks run as in a program (R10) */
    recover_point = 0; /* a runtime error outside a handler ends the program: the parent's to do */
    task_recover_base = 0;
    tr_task_child();
    rcd_task_child();
    deadline_ms = opt_timeout ? sys_clock_ms() + opt_timeout * 1000 : 0; /* the setup's time (R5) */
    Variant *v = (Variant *)call_value(sw_setup, sw_call, 0);
    deadline_ms = 0;
    worker_flush();
    if (v->tag) {
        Str *e = v->pay.s;
        task_send(TW_SETUP_FAILED, e->data, e->len);
        vo_syscall1(SYS_exit_group, 0);
    }
    sw_state = v->pay.i;
    task_send(TW_READY, 0, 0);
    for (;;) {
        long head[2]; /* number, length */
        if (!read_full(req, (char *)head, sizeof head)) vo_syscall1(SYS_exit_group, 0); /* the server stops */
        Str *raw = str_new("", 0);
        if (head[1] > 0) {
            raw = alloc_obj(OBJ_STR, head[1] + 9);
            raw->len = head[1];
            if (!read_full(req, raw->data, head[1])) vo_syscall1(SYS_exit_group, 0);
        }
        GC_ROOT(&raw);
        rt_request_seed(head[0]); /* R8 */
        if (rcd_mode == 1) { /* the request's block starts (and is up before the handler runs) */
            rcd_event(EVK_SERVE_REQUEST, 0, -1, raw, 0);
            rcd_task_end();
        }
        long vals[2] = {sw_state, (long)raw};
        Variant *r = rt_serve_call_n(sw_answer, vals, 2, sw_call);
        sv_take_end(); /* spec 063: a connection the handler took and left open is closed */
        int ok = r->tag == 0;
        Str *text = ok ? r->pay.s : sw_500();
        GC_ROOT(&text);
        if (rcd_mode == 1) rcd_event(EVK_SERVE_RESPONSE, 0, -1, text, 0);
        worker_flush();
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_byte(sb, (char)ok);
        sb_bytes(sb, text->data, text->len);
        task_send(TW_RESPONSE, sb->ptr, sb->len);
        GC_UNROOT(3);
    }
}

/* ---- the parent ---- */

/* Recording (R9): request blocks are written in arrival order; a finished block waits here until the ones
 * before it are written. ponytail: a ring of HOLD blocks; a block more than HOLD requests ahead of the
 * oldest unfinished one is written at once, out of order. */
#define HOLD 4096
static Str *sw_held[HOLD]; /* (globals are collector roots) */
static long sw_written;    /* the next request number to write */

static void sw_write_held(void)
{
    for (Str *b; (b = sw_held[sw_written % HOLD]); sw_written++) {
        rcd_take(b->data, b->len);
        sw_held[sw_written % HOLD] = 0;
    }
}

static void sw_hold(long number, SB *events)
{
    Str *b = str_new(events->ptr, events->len);
    events->len = 0;
    if (number - sw_written >= HOLD) {
        rcd_take(b->data, b->len);
        return;
    }
    sw_held[number % HOLD] = b;
    sw_write_held();
}

/* Every block still held, in order, skipping requests that never finished (a second stop signal). */
static void sw_write_all_held(void)
{
    for (long k = 0; k < HOLD; k++, sw_written++) {
        Str *b = sw_held[sw_written % HOLD];
        if (b) rcd_take(b->data, b->len);
        sw_held[sw_written % HOLD] = 0;
    }
}

/* A worker's setup block (R9): its events between `setup` and `setup_end`. */
static void sw_setup_block(Worker *w, long index, int end)
{
    if (rcd_mode != 1) return;
    rcd_event(EVK_SETUP, 0, -1, 0, index);
    rcd_take(w->events->ptr, w->events->len);
    w->events->len = 0;
    if (end) rcd_event(EVK_SETUP_END, 0, -1, 0, 0);
}

static void sw_close(int *fd)
{
    if (*fd >= 0) sys_close(*fd);
    *fd = -1;
}

static int sw_spawn(Worker *w, long listener)
{
    int p[10];
    for (int k = 0; k < 4; k++)
        if (sys_pipe(&p[2 * k]) < 0) {
            for (int j = 0; j < 2 * k; j++) sys_close(p[j]);
            return 0;
        }
    if (vo_syscall4(SYS_socketpair, AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, (long)&p[8]) < 0) { /* spec 063 */
        for (int j = 0; j < 8; j++) sys_close(p[j]);
        return 0;
    }
    w->pair = p[9];
    for (int k = 0; k < 4; k++) sys_pipe_size(p[2 * k + 1], PIPE_BYTES);
    out_flush(); /* nothing pending is inherited and written twice */
    tr_flush();
    rcd_flush();
    long parent = sys_getpid();
    long pid = sys_fork();
    if (pid == 0) {
        w->pair = p[9];
        worker_run(w, p, listener, parent);
    }
    sys_close(p[8]);
    sys_close(p[0]);
    sys_close(p[3]);
    sys_close(p[5]);
    sys_close(p[7]);
    if (pid < 0) {
        sys_close(p[1]);
        sys_close(p[2]);
        sys_close(p[4]);
        sys_close(p[6]);
        sys_close(p[9]);
        w->pair = -1;
        return 0;
    }
    w->pid = pid;
    w->req = p[1];
    w->ctl = p[2];
    w->out = p[4];
    w->err = p[6];
    w->state = W_STARTING;
    w->conn = -1;
    w->talking = 0;
    if (!w->msgs) w->msgs = sb_new();
    if (!w->obuf) w->obuf = sb_new();
    if (!w->ebuf) w->ebuf = sb_new();
    if (!w->events) w->events = sb_new();
    w->msgs->len = w->obuf->len = w->ebuf->len = w->events->len = 0;
    return 1;
}

/* Reads what fd has into sb; 0 at end of file (fd closed). */
static void sw_read(int *fd, SB *sb)
{
    char buf[65536];
    long n = sys_read(*fd, buf, sizeof buf);
    if (n == -4 || n == -11) return;
    if (n <= 0) {
        sw_close(fd);
        return;
    }
    sb_bytes(sb, buf, n);
}

/* Everything the worker wrote to stdout and stderr up to now (it flushed before its message), written
 * whole. */
static void sw_output(Worker *w)
{
    while (w->out >= 0 && sys_wait_fd(w->out, POLLIN, 0)) sw_read(&w->out, w->obuf);
    while (w->err >= 0 && sys_wait_fd(w->err, POLLIN, 0)) sw_read(&w->err, w->ebuf);
    if (w->obuf->len) {
        if (cap_on) cap_bytes(w->obuf->ptr, w->obuf->len);
        else out_bytes(1, w->obuf->ptr, w->obuf->len);
    }
    if (w->ebuf->len) out_bytes(2, w->ebuf->ptr, w->ebuf->len);
    if (w->obuf->len || w->ebuf->len) out_flush(); /* a server's output appears as its requests end */
    w->obuf->len = w->ebuf->len = 0;
}

/* Every worker ends: with kill, at once; otherwise its request pipe closes and it returns from its loop. */
static void sw_stop_all(int kill)
{
    for (long i = 0; i < sw_n; i++) {
        Worker *w = &sw[i];
        if (w->state == W_NONE) continue;
        if (kill) sys_kill(w->pid);
        sw_close(&w->req);
    }
    for (long i = 0; i < sw_n; i++) {
        Worker *w = &sw[i];
        if (w->state == W_NONE) continue;
        int st;
        sys_wait(w->pid, &st);
        if (!kill) sw_output(w);
        sw_close(&w->ctl);
        sw_close(&w->out);
        sw_close(&w->err);
        w->state = W_NONE;
        sw_close(&w->pair);
    }
}

static unsigned long sw_old_mask;

static void sw_pipe_signals(int on)
{
    unsigned long pipe_sig = 1UL << (SIGPIPE - 1);
    if (on) {
        vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&pipe_sig, (long)&sw_old_mask, 8);
        return;
    }
    /* a SIGPIPE from writing to a worker that ended is pending: consume it before unblocking */
    int s = (int)vo_syscall4(SYS_signalfd4, -1, (long)&pipe_sig, 8, O_NONBLOCK | O_CLOEXEC);
    char info[128];
    while (s >= 0 && sys_read(s, info, sizeof info) > 0) {}
    if (s >= 0) sys_close(s);
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&sw_old_mask, 0, 8);
}

/* The end of serving: workers stopped, connections finished, signals back; result err(failure) or ok(). */
static Variant *sw_finish(long listener, int kill, Str *failure)
{
    GC_ROOT(&failure);
    sw_stop_all(kill);
    if (rcd_mode == 1) sw_write_all_held();
    sv_end(listener);
    sw_pipe_signals(0);
    GC_UNROOT(1);
    return failure ? make_result(1, (long)failure) : make_result(0, 0);
}

/* The worker's complete messages. Returns 0, or what ended the server: 1 a setup failed (*failure),
 * 2 the program ended (*status). */
static int sw_messages(Worker *w, long index, Str **failure, long *status)
{
    SB *m = w->msgs;
    long at = 0;
    int stop = 0;
    while (m->len - at >= 9) {
        int kind = (unsigned char)m->ptr[at];
        long len;
        mem_copy((char *)&len, m->ptr + at + 1, 8);
        if (m->len - at - 9 < len) break;
        char *p = m->ptr + at + 9;
        at += 9 + len;
        switch (kind) {
        case TM_TRACE: tr_take(p, len, 0); break;
        case TM_RECORD: sb_bytes(w->events, p, len); break;
        case TM_COUNTS: break;
        case TM_EXIT: /* the recording gets what is finished, then this worker's events */
            mem_copy((char *)status, p, 8);
            sw_output(w);
            if (rcd_mode == 1) {
                if (w->state == W_STARTING) rcd_event(EVK_SETUP, 0, -1, 0, index);
                sw_write_all_held();
                rcd_take(w->events->ptr, w->events->len);
            }
            stop = 2;
            break;
        case TW_READY:
            sw_output(w);
            sw_setup_block(w, index, 1);
            w->state = W_READY;
            break;
        case TW_SETUP_FAILED:
            sw_output(w);
            sw_setup_block(w, index, 1);
            *failure = str_new(p, len);
            stop = 1;
            break;
        case TW_TAKE: { /* spec 063: the handler asks for its connection (research R1, R3) */
            long ask[2];
            mem_copy((char *)ask, p, 16);
            long talking = 0;
            for (long i = 0; i < sw_n; i++) talking += sw[i].talking;
            long bound = ask[1] > 0 ? ask[1] : sw_n / 2;
            if (bound > sw_n - 1) bound = sw_n - 1; /* never the last free worker */
            char *why = 0;
            long fd = -1;
            if (w->state == W_BUSY && !w->talking && talking < bound) fd = sv_give(w->conn, &why);
            if (fd >= 0) {
                sw_send_fd(w->pair, 1, fd);
                sys_close((int)fd);
                w->conn = -1;
                w->talking = 1;
                sw_gauge();
            } else {
                sw_send_fd(w->pair, why && why[0] == 'b' ? 3 : why ? 4 : 2, -1);
            }
            break;
        }
        case TW_RESPONSE: {
            sw_answered++;
            if (w->talking) {
                w->talking = 0;
                sw_gauge();
            }
            sw_output(w);
            Str *text = str_new(p + 1, len - 1);
            GC_ROOT(&text);
            if (rcd_mode == 1) sw_hold(w->number, w->events);
            sv_reply_to(w->conn, text);
            GC_UNROOT(1);
            w->conn = -1;
            w->state = W_READY;
            break;
        }
        default: break;
        }
        if (stop) break;
    }
    mem_copy(m->ptr, m->ptr + at, m->len - at);
    m->len -= at;
    return stop;
}

/* Replay (R9): in this process, one request at a time: the setup on the first `setup` block, each request
 * on its own block (the other setup blocks skipped), responses compared as serve's are. */
static Variant *sw_replay(void)
{
    rp_take(EVK_SETUP, 0);
    Variant *v = (Variant *)call_value(sw_setup, sw_call, 0);
    GC_ROOT(&v);
    rp_take(EVK_SETUP_END, 0);
    if (!v->tag) {
        for (long number = 0; rp_skip_setups(); number++) {
            Variant *req = io_serve_next(0);
            if (rp_block_ended()) continue;
            rt_request_seed(number);
            long vals[2] = {v->pay.i, (long)req->pay.s};
            Variant *r = rt_serve_call_n(sw_answer, vals, 2, sw_call);
            io_serve_reply(r->tag ? sw_500() : r->pay.s);
        }
        serve_deadline(1);
        v = make_result(0, 0);
    }
    GC_UNROOT(1);
    return v;
}

/* result[void]: serves on the listener the program opened last (__serve_listen) with `workers`
 * processes (R1-R6). */
Variant *rt_serve_workers(long workers, void *setup, void *answer, Node *call)
{
    long listener = sv_listener;
    if (opt_workers) workers = opt_workers;
    if (workers < 1 || workers > WORKER_MAX) {
        if (rcd_mode != 2) sv_end(listener);
        return make_result(1, (long)msg("workers must be 1 to 64, got $1", int_to_str(workers), 0, 0));
    }
    sw_setup = setup;
    sw_answer = answer;
    sw_call = call;
    rt_seed_mark();
    if (rcd_mode == 2) return sw_replay();
    rt_metrics_share(call); /* spec 055: workers forked from here on share one metrics registry */
    sw_n = workers;
    sw_written = 0;
    sw_pipe_signals(1);
    for (long i = 0; i < sw_n; i++)
        if (!sw_spawn(&sw[i], listener)) {
            sw_n = i;
            return sw_finish(listener, 1, str_from_cstr("cannot start a worker (the system refused a process)"));
        }
    int starting = 1; /* until every first worker is ready */
    int stopping = 0, hard = 0, signalled = 0; /* hard: a second signal */
    long drain_end = 0, answered_before = 0, cut = 0; /* spec 055 */
    long number = 0; /* the next request's arrival number */
    Str *failure = 0;
    GC_ROOT(&failure);
    SvPoll ex[SV_EXTRA_MAX];
    SB **into[SV_EXTRA_MAX];
    int *fds[SV_EXTRA_MAX];
    for (;;) {
        if (starting) {
            starting = 0;
            for (long i = 0; i < sw_n; i++) starting |= sw[i].state == W_STARTING;
        }
        long busy = 0;
        for (long i = 0; i < sw_n; i++) {
            Worker *w = &sw[i];
            if (w->state == W_READY && !starting) {
                Str *req;
                long c = sv_take_ready(&req);
                if (c >= 0) {
                    GC_ROOT(&req);
                    long head[2] = {number, req->len};
                    w->number = number++;
                    w->conn = c;
                    w->state = W_BUSY;
                    write_all(w->req, (char *)head, sizeof head);
                    write_all(w->req, req->data, req->len);
                    GC_UNROOT(1);
                }
            }
            busy += w->state == W_BUSY;
        }
        if (signalled && !drain_end) { /* spec 055: the drain is bounded by --drain seconds */
            drain_end = sys_clock_ms() + opt_drain * 1000;
            sv_wake_at = drain_end;
            answered_before = sw_answered;
        }
        if (drain_end && sys_clock_ms() >= drain_end && (busy || sv_pending())) { /* the limit: cut off */
            for (long i = 0; i < sw_n; i++) {
                Worker *w = &sw[i];
                if (w->state != W_BUSY) continue;
                int st;
                sys_kill(w->pid);
                sys_wait(w->pid, &st);
                sw_output(w);
                char *line = "{\"v\":1,\"kind\":\"worker_ended\"}\n{\"v\":1,\"kind\":\"response\",\"value\":\"HTTP/1.1 503 Service Unavailable\\r\\n"
                             "Content-Length: 0\\r\\nConnection: close\\r\\n\\r\\n\"}\n";
                sb_cstr(w->events, line);
                if (rcd_mode == 1) sw_hold(w->number, w->events);
                sv_reply_to(w->conn, str_from_cstr("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
                sw_close(&w->req);
                sw_close(&w->ctl);
                sw_close(&w->out);
                sw_close(&w->err);
                w->state = W_NONE;
                sw_close(&w->pair);
                cut++;
            }
            cut += sv_refuse_ready();
            break;
        }
        if (stopping && !busy && !sv_pending()) break;
        long n = 0;
        for (long i = 0; i < sw_n; i++) {
            Worker *w = &sw[i];
            int *f[3] = {&w->ctl, &w->out, &w->err};
            SB **b[3] = {&w->msgs, &w->obuf, &w->ebuf};
            for (int k = 0; k < 3; k++)
                if (*f[k] >= 0) {
                    ex[n].fd = *f[k];
                    ex[n].events = POLLIN;
                    into[n] = b[k];
                    fds[n++] = f[k];
                }
        }
        if (sv_round(listener, !stopping, ex, n)) {
            if (stopping) { /* a second signal: stop at once */
                hard = 1;
                for (long i = 0; i < sw_n; i++) cut += sw[i].state == W_BUSY;
                break;
            }
            stopping = signalled = 1;
            sv_drop_partial();
            sw_wake_talking(); /* spec 063 */
        }
        for (long k = 0; k < n; k++)
            if (ex[k].revents) sw_read(fds[k], *into[k]);
        for (long i = 0; i < sw_n; i++) {
            Worker *w = &sw[i];
            if (w->state == W_NONE) continue;
            long status = 0;
            int stop = sw_messages(w, i, &failure, &status);
            if (stop == 2) { /* the program ends (exit, or a runtime error outside a handler) */
                sw_stop_all(1);
                sv_end(listener);
                sw_pipe_signals(0);
                out_flush();
                vo_exit((int)status);
            }
            if (stop == 1) { /* a setup failed: the worker ends */
                int st;
                sys_wait(w->pid, &st);
                sw_close(&w->req);
                sw_close(&w->ctl);
                sw_close(&w->out);
                sw_close(&w->err);
                w->state = W_NONE;
                sw_close(&w->pair);
                if (starting) {
                    Variant *r = sw_finish(listener, 1, failure);
                    GC_UNROOT(1);
                    return r;
                }
                if (!stopping) sv_drop_partial();
                stopping = 1;
                continue;
            }
            if (w->ctl < 0) { /* it ended without a word: its request answered 500, a new worker */
                int st;
                sys_kill(w->pid);
                sys_wait(w->pid, &st);
                sw_output(w);
                if (w->state == W_BUSY) { /* its block: the request (sent first), what else came, the marker, 500 */
                    char *line = "{\"v\":1,\"kind\":\"worker_ended\"}\n{\"v\":1,\"kind\":\"response\",\"value\":\"HTTP/1.1 500 Internal Server Error\\r\\n"
                                 "Content-Length: 0\\r\\nConnection: close\\r\\n\\r\\n\"}\n";
                    sb_cstr(w->events, line);
                    if (rcd_mode == 1) sw_hold(w->number, w->events);
                    sv_reply_to(w->conn, sw_500());
                }
                sw_close(&w->req);
                sw_close(&w->out);
                sw_close(&w->err);
                w->state = W_NONE;
                sw_close(&w->pair);
                sv_note(msg("worker $1 ended unexpectedly; it was replaced", int_to_str(i + 1), 0, 0));
                if (w->talking) { /* spec 063: its conversation ended with it */
                    w->talking = 0;
                    sw_gauge();
                }
                if (!sw_spawn(w, listener) && !stopping) {
                    if (!failure) failure = str_from_cstr("cannot start a worker (the system refused a process)");
                    stopping = 1;
                    sv_drop_partial();
                }
            }
        }
    }
    Variant *r = sw_finish(listener, hard || cut, failure);
    GC_ROOT(&r); /* the `stopped` record allocates: the result was collected under it */
    if (signalled) sv_stopped(sw_answered - answered_before, cut);
    sv_wake_at = 0;
    GC_UNROOT(2);
    return r;
}
