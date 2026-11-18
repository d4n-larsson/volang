/* concurrent_map (spec 016): each task is a child process, a plain fork of this one (research R1), so
 * it works on copies of everything and runs as an ordinary sequential vo run. The parent copies the
 * output of the lowest unfinished task only (the head), so a call's output is task 0's, then task 1's,
 * ... whatever order they finish in (R5); the failure reported is the one at the lowest position (R6);
 * results come back over a pipe in a form driven by their static type (R4, data-model.md).
 * ponytail: head-first draining, so a task that prints more than its pipe holds (1 MiB) waits until
 * it is the head; buffer the others' output in the parent if that ever matters. */
#include "vo.h"

#define TASK_MAX 64
#define PIPE_BYTES (1L << 20)
#define POLLIN 1
#define POLLERR 8
#define POLLHUP 16
#define GRACE_MS 1000 /* past the deadline, the tasks report R0009 themselves; this is the backstop */

enum { TM_RESULT = 1, TM_JUMP, TM_EXIT, TM_TRACE, TM_RECORD, TM_COUNTS }; /* TM_TRACE and TM_RECORD: fds -4, -5 */

long task_depth;    /* > 0 in a task */
long task_ctl = -1; /* in a task: the control pipe to the parent */
long task_path;     /* in a task: its position path (research R12) */
long task_recover_base; /* in a task: the recovery point it inherited (the parent's, R6) */

typedef struct Slot {
    long pos;  /* the task's position in xs, -1 when the slot is free */
    long pid;
    int fd[3]; /* read ends: stdout, stderr, control (-1 once at end of file) */
    int exited; /* reaped already (its pipes may still hold data) */
    int status; /* its wait status, when exited */
} Slot;

typedef struct Group {
    Slot slots[TASK_MAX];
    long running; /* slots in use and not yet reaped */
    long failed;  /* the lowest position known to have failed, or -1 */
} Group;

typedef struct PollFd {
    int fd;
    short events, revents;
} PollFd;

static void slot_close(Slot *s)
{
    for (int k = 0; k < 3; k++)
        if (s->fd[k] >= 0) {
            sys_close(s->fd[k]);
            s->fd[k] = -1;
        }
}

/* Every task still in the group is killed and reaped (a failure below it, a timeout, an internal
 * error): no task outlives the call (FR-003). */
static void group_stop(Group *g, long above)
{
    for (int i = 0; i < TASK_MAX; i++) {
        Slot *s = &g->slots[i];
        if (s->pos < 0 || s->pos <= above) continue;
        if (!s->exited) {
            sys_kill(s->pid);
            int st;
            sys_wait(s->pid, &st);
            g->running--;
        }
        slot_close(s);
        s->pos = -1;
    }
}

static void task_internal(Group *g, char *what) NORETURN;
static void task_internal(Group *g, char *what)
{
    group_stop(g, -1);
    out_flush();
    toolchain_error((int)opt_json, 2, "T0003", str_from_cstr(what), 0);
    vo_exit(EXIT_INTERNAL);
    for (;;) {}
}

/* ---- the transfer form (research R4) ---- */

static void put_word(SB *sb, long w)
{
    sb_bytes(sb, (char *)&w, 8);
}

/* A Str, or -1 for none. */
static void put_str(SB *sb, Str *s)
{
    if (!s) {
        put_word(sb, -1);
        return;
    }
    put_word(sb, s->len);
    sb_bytes(sb, s->data, s->len);
}

static int is_ref(Type *t)
{
    switch (t->kind) {
    case TY_STR:
    case TY_BYTES: /* spec 021 */
    case TY_LIST:
    case TY_MAP:
    case TY_RECORD:
    case TY_VARIANT:
    case TY_RESULT:
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return 1;
    default: return 0;
    }
}

static Map *enc_seen; /* in the child: object address -> index (the parent's table, R4) */
static long enc_count;
static Node *enc_call;

/* Appends value of type t (concrete) to sb. */
static void enc_value(SB *sb, long value, Type *t)
{
    POLL(1);
    check_stack(enc_call); /* deep values are R0005, as for == */
    t = rt_concrete(t);
    if (t->kind == TY_VOID) return;
    if (!is_ref(t)) {
        if (t->kind != TY_INT && t->kind != TY_FLOAT && t->kind != TY_BOOL)
            rt_raise_c("R0015", "A task's result cannot hold a function.", enc_call); /* (E0314 prevents it) */
        put_word(sb, value);
        return;
    }
    long seen = map_find(enc_seen, value, t_int);
    if (seen >= 0) { /* a shared object: its index */
        put_word(sb, enc_seen->entries[seen].val.i);
        return;
    }
    map_set(enc_seen, value, enc_count++, t_int);
    put_word(sb, -1); /* a new object follows */
    switch (t->kind) {
    case TY_STR:
    case TY_BYTES: put_str(sb, (Str *)value); return; /* spec 021: length and data, any bytes */
    case TY_LIST: {
        List *l = (List *)value;
        put_word(sb, l->len);
        for (long i = 0; i < l->len; i++) enc_value(sb, l->data[i].i, t->a.t);
        return;
    }
    case TY_MAP: {
        Map *m = (Map *)value;
        put_word(sb, m->count);
        for (long i = 0; i < m->used; i++) {
            if (!m->entries[i].live) continue;
            enc_value(sb, m->entries[i].key.i, t->a.t);
            enc_value(sb, m->entries[i].val.i, t->b.t);
        }
        return;
    }
    case TY_RECORD: {
        long n = pg_nmembers(t);
        for (long i = 0; i < n; i++) enc_value(sb, ((Record *)value)->fields[i].i, pg_member(t, i));
        return;
    }
    case TY_VARIANT:
    case TY_RESULT: {
        Variant *v = (Variant *)value;
        long tag = (long)(uint32_t)v->tag;
        put_word(sb, tag);
        Type *pt = t->kind == TY_RESULT ? case_payload(t, tag) : pg_member(t, tag);
        if (pt) enc_value(sb, v->pay.i, pt);
        return;
    }
    default: { /* the collections: their items in order, rebuilt with the built-in order */
        Coll *c = (Coll *)value;
        if (c->cmp)
            rt_raise_c("R0015", "A task cannot return a collection with its own order: return its items (`keys`) instead.",
                       enc_call);
        GC_ROOT(&c);
        List *xs = coll_snapshot(c);
        GC_ROOT(&xs);
        List *vs = t->kind == TY_OMAP ? coll_values(c) : 0;
        GC_ROOT(&vs);
        put_word(sb, xs->len);
        for (long i = 0; i < xs->len; i++) {
            enc_value(sb, xs->data[i].i, t->a.t);
            if (vs) enc_value(sb, vs->data[i].i, t->b.t);
        }
        GC_UNROOT(3);
        return;
    }
    }
}

typedef struct Reader {
    char *p;
    long n, at;
    Group *g;
} Reader;

static long get_word(Reader *r)
{
    if (r->n - r->at < 8) task_internal(r->g, "a task's message is cut short");
    long w;
    mem_copy((char *)&w, r->p + r->at, 8);
    r->at += 8;
    return w;
}

/* A Str (or 0 for none) from the message. */
static Str *get_str(Reader *r)
{
    long n = get_word(r);
    if (n == -1) return 0;
    if (n < 0 || n > r->n - r->at) task_internal(r->g, "a task's message is cut short");
    Str *s = str_new(r->p + r->at, n);
    r->at += n;
    return s;
}

static List *dec_objs; /* in the parent: the objects decoded so far, by index */

static long dec_value(Reader *r, Type *t)
{
    t = rt_concrete(t);
    if (t->kind == TY_VOID) return 0;
    if (!is_ref(t)) return get_word(r);
    long idx = get_word(r);
    if (idx >= 0) {
        if (idx >= dec_objs->len) task_internal(r->g, "a task's result refers to an unknown object");
        return dec_objs->data[idx].i;
    }
    long slot = dec_objs->len; /* the object goes in before its contents: cycles find it */
    list_push(dec_objs, 0);
    switch (t->kind) {
    case TY_STR:
    case TY_BYTES: { /* spec 021 */
        Str *s = get_str(r);
        if (!s) task_internal(r->g, "a task's message is cut short");
        dec_objs->data[slot].i = (long)s;
        return (long)s;
    }
    case TY_LIST: {
        long n = get_word(r);
        if (n < 0 || n > r->n) task_internal(r->g, "a task's message is cut short");
        List *l = list_new(n ? n : 1);
        dec_objs->data[slot].i = (long)l;
        for (long i = 0; i < n; i++) {
            long v = dec_value(r, t->a.t);
            list_push(l, v);
        }
        return (long)l;
    }
    case TY_MAP: {
        long n = get_word(r);
        Map *m = map_new();
        dec_objs->data[slot].i = (long)m;
        for (long i = 0; i < n; i++) {
            long k = dec_value(r, t->a.t);
            GC_ROOT(&k);
            long v = dec_value(r, t->b.t);
            map_set(m, k, v, t->a.t);
            GC_UNROOT(1);
        }
        return (long)m;
    }
    case TY_RECORD: {
        long n = pg_nmembers(t);
        Record *rec = alloc_obj(OBJ_RECORD, n * 8);
        dec_objs->data[slot].i = (long)rec;
        for (long i = 0; i < n; i++) {
            long v = dec_value(r, pg_member(t, i));
            rec->fields[i].i = v;
        }
        return (long)rec;
    }
    case TY_VARIANT:
    case TY_RESULT: {
        long tag = get_word(r);
        Type *pt = t->kind == TY_RESULT ? case_payload(t, tag) : pg_member(t, tag);
        Variant *v = pg_new_variant(tag, 0);
        dec_objs->data[slot].i = (long)v;
        if (pt) {
            long pay = dec_value(r, pt);
            v->pay.i = pay;
        }
        return (long)v;
    }
    default: {
        long n = get_word(r);
        if (n < 0 || n > r->n) task_internal(r->g, "a task's message is cut short");
        List *xs = list_new(n ? n : 1);
        GC_ROOT(&xs);
        List *vs = t->kind == TY_OMAP ? list_new(n ? n : 1) : 0;
        GC_ROOT(&vs);
        Coll *c = coll_new(t, 0); /* registered first; filled below */
        dec_objs->data[slot].i = (long)c;
        for (long i = 0; i < n; i++) {
            long k = dec_value(r, t->a.t);
            list_push(xs, k);
            if (vs) {
                long v = dec_value(r, t->b.t);
                list_push(vs, v);
            }
        }
        for (long i = 0; i < n; i++) {
            if (vs) coll_set_index(c, xs->data[i].i, vs->data[i].i, 0);
            else coll_push(c, xs->data[i].i);
        }
        GC_UNROOT(2);
        return (long)c;
    }
    }
}

/* ---- messages (data-model.md "Control messages") ---- */

/* In a task: sends one message (kind, length, payload) to the parent. */
void task_send(int kind, char *p, long n)
{
    char head[9];
    head[0] = (char)kind;
    mem_copy(head + 1, (char *)&n, 8);
    write_all((int)task_ctl, head, 9);
    write_all((int)task_ctl, p, n);
}

/* In a task: the end. Pending output, the rest of the trace and the recording, the counts, then the
 * last message (kind); the process ends with 0 after a result, 1 after a failure. */
static void task_final(int kind, char *p, long n) NORETURN;
static void task_final(int kind, char *p, long n)
{
    deadline_ms = 0; /* nothing may raise any more */
    out_flush();
    long counts[2] = {tr_task_end(), rp_position()};
    rcd_task_end();
    task_send(TM_COUNTS, (char *)counts, sizeof counts);
    task_send(kind, p, n);
    vo_syscall1(SYS_exit_group, kind == TM_RESULT ? 0 : 1);
    for (;;) {}
}

/* In a task: the program would end with status (vo_exit). The parent ends it the same way once every
 * task before this one has finished (R6). */
void task_exit(long status)
{
    task_depth = 0; /* (vo_exit's hooks never run here) */
    task_final(TM_EXIT, (char *)&status, 8);
}

/* In a task: a runtime error or a failed expect would return to the recovery point (a test, a
 * property case or a server's handler). The parent returns there instead, with the same state (R6). */
void task_jump(long status)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    put_word(sb, status);
    if (status == 1) { /* expect */
        put_word(sb, tf_node);
        put_str(sb, (Str *)tf_msg);
        put_str(sb, (Str *)tf_expected);
        put_str(sb, (Str *)tf_actual);
    } else {
        put_word(sb, rt_err_code); /* static strings and AST nodes: the same addresses in the parent */
        put_word(sb, rt_err_node);
        put_str(sb, (Str *)rt_err_msg);
        put_str(sb, (Str *)rt_err_extra);
        put_str(sb, (Str *)rt_err_trace_txt);
        put_str(sb, (Str *)rt_err_trace_json);
    }
    task_final(TM_JUMP, sb->ptr, sb->len);
}

/* The parent: what a failed task asked for, now that it is the lowest failure (R6). */
static void task_fail(Group *g, Reader *r) NORETURN;
static void task_fail(Group *g, Reader *r)
{
    long kind = (unsigned char)r->p[r->at - 9]; /* the message's kind byte */
    long status = get_word(r);
    if (kind == TM_EXIT) {
        group_stop(g, -1);
        gc_report(2, "run", 0); /* --mem-stats: the run ends here, as after `map` */
        vo_exit((int)status);
    }
    if (status == 1) {
        tf_node = get_word(r);
        tf_msg = (long)get_str(r);
        tf_expected = (long)get_str(r);
        tf_actual = (long)get_str(r);
    } else {
        rt_err_code = get_word(r);
        rt_err_node = get_word(r);
        rt_err_msg = (long)get_str(r);
        rt_err_extra = (long)get_str(r);
        rt_err_trace_txt = (long)get_str(r);
        rt_err_trace_json = (long)get_str(r);
        rt_err_in_test = 0; /* spec 018: a task always runs a function */
    }
    group_stop(g, -1);
    if (!recover_point) task_internal(g, "a task returned to a recovery point that does not exist");
    jmp_restore((void *)recover_point, status);
}

/* ---- the call ---- */

static void child_run(Group *g, int *pipes, long pos, List *xs, Closure *fn, Node *call, long parent) NORETURN;
static void child_run(Group *g, int *pipes, long pos, List *xs, Closure *fn, Node *call, long parent)
{
    sys_die_with_parent(parent);
    for (int i = 0; i < TASK_MAX; i++) slot_close(&g->slots[i]); /* the other tasks' pipes */
    sys_dup_to(pipes[1], 1);
    sys_dup_to(pipes[3], 2);
    sys_close(pipes[0]);
    sys_close(pipes[1]);
    sys_close(pipes[2]);
    sys_close(pipes[3]);
    sys_close(pipes[4]);
    task_ctl = pipes[5];
    cap_on = 0; /* its output goes up the pipe; the parent captures it (spec 018) */
    task_depth++;
    task_path = task_path * (TASK_MAX + 1) + pos + 1;
    task_recover_base = recover_point;
    rt_task_seed(task_path); /* its own random sequence (R12) */
    tr_task_child();         /* trace records and recorded events go through the control pipe */
    rcd_task_child();
    long r = call_value(fn, call, &xs->data[pos].i);
    GC_ROOT(&r);
    enc_call = call;
    enc_seen = map_new();
    GC_ROOT(&enc_seen);
    enc_count = 0;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    enc_value(sb, r, rt_concrete(call->type)->a.t);
    task_final(TM_RESULT, sb->ptr, sb->len);
}

static Slot *slot_of(Group *g, long pos)
{
    for (int i = 0; i < TASK_MAX; i++)
        if (g->slots[i].pos == pos) return &g->slots[i];
    return 0;
}

static void task_start(Group *g, long pos, List *xs, Closure *fn, Node *call)
{
    Slot *s = slot_of(g, -1);
    if (!s) task_internal(g, "no free task slot");
    int p[6];
    for (int k = 0; k < 3; k++)
        if (sys_pipe(&p[2 * k]) < 0) task_internal(g, "cannot create a pipe for a task");
    for (int k = 0; k < 3; k++) sys_pipe_size(p[2 * k + 1], PIPE_BYTES);
    out_flush(); /* nothing pending is inherited and written twice */
    tr_flush();
    rcd_flush();
    long parent = sys_getpid();
    long pid = sys_fork();
    if (pid < 0) task_internal(g, "cannot start a task (the system refused to create a process)");
    if (pid == 0) child_run(g, p, pos, xs, fn, call, parent);
    s->pos = pos;
    s->pid = pid;
    s->exited = 0;
    s->status = 0;
    for (int k = 0; k < 3; k++) {
        s->fd[k] = p[2 * k];
        sys_close(p[2 * k + 1]);
    }
    g->running++;
}

/* Reads what fd has: 0 at end of file (the fd is closed), 1 otherwise. stdout and stderr go to ours. */
static int drain(Slot *s, int k, SB *ctl)
{
    char buf[65536];
    long n = sys_read(s->fd[k], buf, sizeof buf);
    if (n == -4 || n == -11) return 1; /* EINTR, EAGAIN */
    if (n <= 0) {
        sys_close(s->fd[k]);
        s->fd[k] = -1;
        return 0;
    }
    if (k == 0 && cap_on) cap_bytes(buf, n); /* spec 018: a test's task output stays with the test */
    else if (k < 2) out_bytes(k + 1, buf, n);
    else sb_bytes(ctl, buf, n);
    return 1;
}

static void reap(Group *g, Slot *s)
{
    int st = 0;
    sys_wait(s->pid, &st);
    s->exited = 1;
    s->status = st;
    g->running--;
    if (st != 0 && (g->failed < 0 || s->pos < g->failed)) {
        g->failed = s->pos;
        group_stop(g, s->pos); /* the tasks above it will not be reported */
    }
}

/* The head's control messages, in order; its result, or a failure (which does not return). */
static long take_messages(Group *g, SB *ctl, Type *elem, long index)
{
    tr_task_record(index); /* the trace's `task` record (R11) */
    Reader r = {ctl->ptr, ctl->len, 0, g};
    long result = 0;
    int got = 0;
    while (r.at < r.n) {
        if (r.n - r.at < 9) task_internal(g, "a task's message is cut short");
        int kind = (unsigned char)r.p[r.at];
        long len;
        mem_copy((char *)&len, r.p + r.at + 1, 8);
        r.at += 9;
        if (len < 0 || len > r.n - r.at) task_internal(g, "a task's message is cut short");
        long end = r.at + len;
        switch (kind) {
        case TM_RESULT: {
            Reader body = {r.p, end, r.at, g};
            dec_objs = list_new(8); /* (rooted by task_map) */
            result = dec_value(&body, elem);
            dec_objs = 0;
            got = 1;
            break;
        }
        case TM_JUMP:
        case TM_EXIT: {
            Reader body = {r.p, end, r.at, g};
            task_fail(g, &body);
        }
        case TM_TRACE: tr_take(r.p + r.at, len, 0); break;
        case TM_RECORD: rcd_take(r.p + r.at, len); break;
        case TM_COUNTS: {
            if (len != 16) task_internal(g, "a task's message is cut short");
            long counts[2];
            mem_copy((char *)counts, r.p + r.at, 16);
            tr_take(0, 0, counts[0]);
            rp_set_position(counts[1]);
            break;
        }
        default: task_internal(g, "a task sent an unknown message");
        }
        r.at = end;
    }
    if (!got) task_internal(g, "a task ended without a result");
    return result;
}

List *task_map(List *xs, long limit, Closure *fn, Node *call)
{
    if (limit < 1 || limit > TASK_MAX)
        rt_raise("R0007", msg("The task limit $1 is outside 1-64.", int_to_str(limit), 0, 0), call);
    /* ponytail: inside a task (and in replay) tasks run one at a time; a jobserver pipe inherited by
       the tasks if nested concurrency is ever needed (research R7, R10) */
    if (task_depth || rcd_mode == 2) limit = 1;
    GC_ROOT(&xs);
    GC_ROOT(&fn);
    Type *elem = rt_concrete(call->type)->a.t;
    List *out = list_new(xs->len ? xs->len : 1);
    GC_ROOT(&out);
    SB *ctl = sb_new();
    GC_ROOT(&ctl);
    List *saved_objs = dec_objs; /* (a task_map inside a callback of a decode cannot happen, but keep it) */
    GC_ROOT(&saved_objs);
    GC_ROOT(&dec_objs);
    Group g;
    for (int i = 0; i < TASK_MAX; i++) {
        g.slots[i].pos = -1;
        for (int k = 0; k < 3; k++) g.slots[i].fd[k] = -1;
    }
    g.running = 0;
    g.failed = -1;
    long n = xs->len, next = 0, head = 0;
    while (head < n) {
        /* invariant: at most `limit` tasks running; no task above a known failure starts. A task that
           has ended but is not drained yet keeps its slot (its output waits in its pipes). */
        while (next < n && g.running < limit && g.failed < 0 && slot_of(&g, -1)) task_start(&g, next++, xs, fn, call);
        Slot *h = slot_of(&g, head);
        if (!h) task_internal(&g, "the next task to report is missing");
        PollFd p[TASK_MAX + 3];
        Slot *owner[TASK_MAX + 3];
        int kind[TASK_MAX + 3];
        long np = 0;
        for (int k = 0; k < 3; k++)
            if (h->fd[k] >= 0) {
                p[np] = (PollFd){h->fd[k], POLLIN, 0};
                owner[np] = h;
                kind[np++] = k;
            }
        for (int i = 0; i < TASK_MAX; i++) { /* the others: only whether they have ended (POLLHUP) */
            Slot *s = &g.slots[i];
            if (s == h || s->pos < 0 || s->exited || s->fd[2] < 0) continue;
            p[np] = (PollFd){s->fd[2], 0, 0};
            owner[np] = s;
            kind[np++] = -1;
        }
        if (np == 0) { /* the head has closed everything: it is done */
            if (!h->exited) reap(&g, h);
            if (g.failed >= 0 && g.failed < head) task_internal(&g, "a failed task was passed over");
            long v = take_messages(&g, ctl, elem, head); /* a failure does not return */
            list_push(out, v);
            ctl->len = 0;
            h->pos = -1;
            head++;
            continue;
        }
        long wait = -1;
        if (deadline_ms) {
            wait = deadline_ms + GRACE_MS - sys_clock_ms();
            if (wait <= 0) {
                group_stop(&g, -1);
                rt_timeout(call);
            }
        }
        long ts[2] = {wait / 1000, wait % 1000 * 1000000};
        long r = vo_syscall5(SYS_ppoll, (long)p, np, wait < 0 ? 0 : (long)ts, 0, 8);
        if (r < 0 && r != -4) task_internal(&g, "cannot wait for the tasks");
        for (long i = 0; i < np && r > 0; i++) {
            if (!p[i].revents) continue;
            if (kind[i] >= 0) drain(owner[i], kind[i], ctl);
            else if (owner[i]->pos >= 0 && !owner[i]->exited) reap(&g, owner[i]);
        }
    }
    dec_objs = saved_objs;
    GC_UNROOT(6);
    return out;
}
