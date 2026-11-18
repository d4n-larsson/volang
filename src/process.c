/* Spec 017: std.process's primitive __run(argv, input, env) (research R8-R11).
 *
 * argv[0] is the program as the caller wrote it. It must match a `process` scope of the running and
 * the root grant exactly (after dropping a leading "./"), then it is resolved: a path with "/" as it is
 * (relative to the start directory, which is the working directory: vo never changes it), a bare name
 * through the PATH of the vo process (absolute entries, the first regular file with an execute bit).
 * The child gets its own process group, three pipes as 0/1/2, an empty signal mask, the given
 * environment only, and dies with vo. An error pipe (closed on exec) tells "could not start" from
 * "started". SIGCHLD through a signalfd wakes the wait; when the child is reaped its whole group is
 * killed and the pipes are read until empty. --timeout and 16 MiB per stream kill the group too.
 * The ok text is "<code>\n<stdout length>\n<stdout><stderr>"; code is 128 + n for signal n.
 * ponytail: a plain fork per run (the page tables are copied); CLONE_VM/CLONE_VFORK if a large heap
 * makes it slow. A grandchild that leaves the group (setsid), or any grandchild after vo itself is
 * SIGKILLed, survives: a PID namespace or cgroup when children are confined (roadmap). */
#include "vo.h"

#define MAX_OUTPUT (16L << 20)
#define SIGPIPE 13
#define SIGCHLD 17
#define SIGKILL 9
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define F_SETFL 4
#define F_SETFD 2
#define POLLIN 1
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 16
#define WNOHANG 1
#define E_AGAIN 11
#define E_NOENT 2
#define E_ACCES 13
#define E_INTR 4
#define STATX_TYPE 0x1
#define STATX_MODE 0x2
#define STATX_MODE_OFF 28

typedef struct PollFd {
    int fd;
    short events;
    short revents;
} PollFd;

static void raise_arg(char *m)
{
    rt_raise("R0007", str_from_cstr(m), (void *)cur_node);
}

static int has_nul(Str *s)
{
    for (long i = 0; i < s->len; i++)
        if (!s->data[i]) return 1;
    return 0;
}

/* 1 when path is a regular file with an execute bit. */
static int executable(char *path)
{
    long stx[32];
    if (vo_syscall5(SYS_statx, AT_FDCWD, (long)path, 0, STATX_TYPE | STATX_MODE, (long)stx) < 0) return 0;
    unsigned mode = *(uint16_t *)((char *)stx + STATX_MODE_OFF);
    return (mode & 0170000) == 0100000 && (mode & 0111);
}

/* The file to execute for program (see the header), or 0 when a bare name is on no PATH entry. */
static Str *resolve_program(Str *program)
{
    for (long i = 0; i < program->len; i++)
        if (program->data[i] == '/') return program;
    char **env = (char **)envp;
    char *path = 0;
    for (; env && *env; env++)
        if (mem_eq(*env, "PATH=", 5)) path = *env + 5;
    GC_ROOT(&program);
    Str *found = 0;
    for (char *p = path; p && *p && !found;) {
        char *end = p;
        while (*end && *end != ':') end++;
        if (*p == '/') {
            SB *sb = sb_new();
            GC_ROOT(&sb);
            sb_bytes(sb, p, end - p);
            sb_byte(sb, '/');
            sb_str(sb, program);
            GC_UNROOT(1);
            Str *cand = sb_finish(sb);
            if (executable(cand->data)) found = cand;
        }
        p = *end ? end + 1 : end;
    }
    GC_UNROOT(1);
    return found;
}

static void set_flags(int fd, long flags)
{
    vo_syscall3(SYS_fcntl, fd, F_SETFL, flags);
}

static void close_fd(int *fd)
{
    if (*fd >= 0) sys_close(*fd);
    *fd = -1;
}

/* Reads what fd has now into sb; 0 at end of input or on error (fd closed), 1 otherwise. *over is set
 * when sb passes the limit. */
static int drain(int *fd, SB *sb, int *over)
{
    static char buf[65536];
    for (;;) {
        long n = sys_read(*fd, buf, sizeof buf);
        if (n == -E_AGAIN) return 1;
        if (n == -E_INTR) continue;
        if (n <= 0) {
            close_fd(fd);
            return 0;
        }
        if (sb->len + n > MAX_OUTPUT) {
            *over = 1;
            return 1;
        }
        sb_bytes(sb, buf, n);
    }
}

/* The child after fork: never returns. */
static void child(int in, int out, int err, int ex, char *path, char **argv, char **envv, long parent)
{
    if (ex < 3) { /* keep the error pipe above the descriptors closed below */
        int moved = (int)vo_syscall3(SYS_fcntl, ex, 1030 /* F_DUPFD_CLOEXEC */, 3);
        if (moved >= 0) ex = moved;
    }
    vo_syscall2(SYS_setpgid, 0, 0);
    sys_die_with_parent(parent);
    unsigned long none = 0;
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&none, 0, 8);
    int from[3] = {in, out, err};
    for (int k = 0; k < 3; k++) {
        if (from[k] == k)
            vo_syscall3(SYS_fcntl, k, F_SETFD, 0); /* already in place: keep it across exec */
        else
            sys_dup_to(from[k], k);
    }
    /* the error pipe as 3 (closed by a successful exec); nothing else reaches the program, not even
       descriptors vo inherited without close-on-exec */
    if (ex != 3) vo_syscall3(SYS_dup3, ex, 3, O_CLOEXEC);
    ex = 3;
    if (vo_syscall3(SYS_close_range, 4, ~0UL, 0) < 0)
        for (int fd = 4; fd < 4096; fd++) sys_close(fd);
    long r = vo_syscall3(SYS_execve, (long)path, (long)argv, (long)envv);
    int code = (int)-r;
    sys_write(ex, &code, sizeof code);
    vo_syscall1(SYS_exit_group, 127);
    for (;;) {}
}

/* Runs a checked program; the result text or err. */
static Variant *rt_run(List *argv, Str *input, List *env, int raw)
{
    Str *program = (Str *)argv->data[0].p;
    GC_ROOT(&argv);
    GC_ROOT(&input);
    GC_ROOT(&env);
    Str *path = resolve_program(program);
    GC_ROOT(&path);
    if (!path) {
        GC_UNROOT(4);
        return make_result(1, (long)str_from_cstr("not found"));
    }
    char **av = alloc_bytes((argv->len + 1) * 8);
    GC_ROOT(&av);
    char **ev = alloc_bytes((env->len + 1) * 8);
    GC_ROOT(&ev);
    for (long i = 0; i < argv->len; i++) av[i] = ((Str *)argv->data[i].p)->data;
    for (long i = 0; i < env->len; i++) ev[i] = ((Str *)env->data[i].p)->data;
    SB *out = sb_new(), *err = 0;
    GC_ROOT(&out);
    GC_ROOT(&err);
    err = sb_new();
    int pin[2] = {-1, -1}, pout[2] = {-1, -1}, perr[2] = {-1, -1}, pex[2] = {-1, -1};
    if (sys_pipe(pin) < 0 || sys_pipe(pout) < 0 || sys_pipe(perr) < 0 || sys_pipe(pex) < 0) {
        close_fd(&pin[0]), close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&pout[1]);
        close_fd(&perr[0]), close_fd(&perr[1]), close_fd(&pex[0]), close_fd(&pex[1]);
        GC_UNROOT(8);
        return make_result(1, (long)str_from_cstr("cannot run: no pipes"));
    }
    unsigned long mask = (1UL << (SIGCHLD - 1)) | (1UL << (SIGPIPE - 1)), old = 0;
    vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&mask, (long)&old, 8);
    unsigned long chld = 1UL << (SIGCHLD - 1);
    int sig = (int)vo_syscall4(SYS_signalfd4, -1, (long)&chld, 8, O_NONBLOCK | O_CLOEXEC);
    long parent = sys_getpid();
    long pid = sys_fork();
    if (pid == 0) child(pin[0], pout[1], perr[1], pex[1], path->data, av, ev, parent);
    close_fd(&pin[0]), close_fd(&pout[1]), close_fd(&perr[1]), close_fd(&pex[1]);
    int failed = 0; /* the errno of a failed exec */
    if (pid > 0) {
        int code = 0;
        long n;
        do {
            n = sys_read(pex[0], &code, sizeof code);
        } while (n == -E_INTR);
        if (n == sizeof code) failed = code ? code : 1;
    }
    close_fd(&pex[0]);
    int status = 0, reaped = 0, over = 0, timed_out = 0;
    long sent = 0;
    if (pid < 0 || failed) {
        if (pid > 0) sys_wait(pid, &status);
        close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&perr[0]);
    } else {
        set_flags(pin[1], O_NONBLOCK);
        set_flags(pout[0], O_NONBLOCK);
        set_flags(perr[0], O_NONBLOCK);
        if (!input->len) close_fd(&pin[1]);
        while (!reaped && !over) {
            PollFd fds[4];
            int nf = 0;
            fds[nf++] = (PollFd){sig, POLLIN, 0};
            if (pout[0] >= 0) fds[nf++] = (PollFd){pout[0], POLLIN, 0};
            if (perr[0] >= 0) fds[nf++] = (PollFd){perr[0], POLLIN, 0};
            if (pin[1] >= 0) fds[nf++] = (PollFd){pin[1], POLLOUT, 0};
            long ts[2], *tp = 0;
            if (deadline_ms) {
                long left = deadline_ms - sys_clock_ms();
                if (left <= 0) {
                    timed_out = 1;
                    break;
                }
                ts[0] = left / 1000;
                ts[1] = (left % 1000) * 1000000;
                tp = ts;
            }
            long r = vo_syscall5(SYS_ppoll, (long)fds, nf, (long)tp, 0, 8);
            if (r < 0 && r != -E_INTR) break;
            if (pout[0] >= 0) drain(&pout[0], out, &over);
            if (perr[0] >= 0) drain(&perr[0], err, &over);
            if (pin[1] >= 0) {
                long w = sys_write(pin[1], input->data + sent, input->len - sent);
                if (w > 0) sent += w;
                if ((w < 0 && w != -E_AGAIN) || sent >= input->len) close_fd(&pin[1]); /* EPIPE: it stopped reading */
            }
            char info[128];
            while (sys_read(sig, info, sizeof info) > 0) {}
            reaped = vo_syscall4(SYS_wait4, pid, (long)&status, WNOHANG, 0) == pid;
        }
        vo_syscall2(SYS_kill, -pid, SIGKILL); /* the child's group: whatever it started */
        if (!reaped) sys_wait(pid, &status);
        if (pout[0] >= 0) drain(&pout[0], out, &over);
        if (perr[0] >= 0) drain(&perr[0], err, &over);
        close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&perr[0]);
    }
    if (sig >= 0) {
        char info[128];
        while (sys_read(sig, info, sizeof info) > 0) {}
        sys_close(sig);
    }
    { /* a SIGPIPE from writing to a child that stopped reading is pending: consume it before unblocking */
        unsigned long pipe_sig = 1UL << (SIGPIPE - 1);
        int s2 = (int)vo_syscall4(SYS_signalfd4, -1, (long)&pipe_sig, 8, O_NONBLOCK | O_CLOEXEC);
        char info[128];
        while (s2 >= 0 && sys_read(s2, info, sizeof info) > 0) {}
        if (s2 >= 0) sys_close(s2);
    }
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&old, 0, 8);
    if (timed_out) rt_timeout((Node *)cur_node); /* (the roots are restored by the jump) */
    Variant *res;
    if (pid < 0) {
        res = make_result(1, (long)msg("cannot run: error $1", int_to_str(-pid), 0, 0));
    } else if (failed == E_NOENT) {
        res = make_result(1, (long)str_from_cstr("not found"));
    } else if (failed == E_ACCES) {
        res = make_result(1, (long)str_from_cstr("permission denied"));
    } else if (failed) {
        res = make_result(1, (long)msg("cannot run: error $1", int_to_str(failed), 0, 0));
    } else if (over) {
        res = make_result(1, (long)str_from_cstr("output is larger than 16 MiB"));
    } else {
        Str *o = sb_finish(out);
        GC_ROOT(&o);
        Str *e = sb_finish(err);
        GC_ROOT(&e);
        if (!raw && (utf8_validate(o->data, o->len) >= 0 || utf8_validate(e->data, e->len) >= 0)) {
            res = make_result(1, (long)str_from_cstr("output is not UTF-8 (run_bytes reads it)")); /* spec 021 */
        } else {
            long code = (status & 0x7f) ? 128 + (status & 0x7f) : (status >> 8) & 0xff;
            SB *sb = sb_new();
            GC_ROOT(&sb);
            int_to_dec(sb, code);
            sb_byte(sb, '\n');
            int_to_dec(sb, o->len);
            sb_byte(sb, '\n');
            sb_str(sb, o);
            sb_str(sb, e);
            res = make_result(0, (long)sb_finish(sb));
            GC_UNROOT(1);
        }
        GC_UNROOT(2);
    }
    GC_UNROOT(8);
    return res;
}

/* Checks the arguments (R0007) and the program against both grants (R0010), then runs it. raw (spec
 * 021, run_bytes): the output is returned as it is, not checked as UTF-8. */
Variant *io_run(List *argv, Str *input, List *env, int raw)
{
    GC_ROOT(&argv);
    GC_ROOT(&input);
    GC_ROOT(&env);
    if (!argv->len) raise_arg("A program to run is needed.");
    Str *program = (Str *)argv->data[0].p;
    if (!program->len || has_nul(program)) raise_arg("A program must be a non-empty name or path without NUL bytes.");
    for (long i = 1; i < argv->len; i++)
        if (has_nul((Str *)argv->data[i].p)) raise_arg("An argument cannot hold a NUL byte.");
    for (long i = 0; i < env->len; i++) {
        Str *v = (Str *)env->data[i].p;
        long eq = 0;
        while (eq < v->len && v->data[eq] != '=') eq++;
        if (eq == 0 || eq == v->len || has_nul(v))
            raise_arg("An environment entry must be NAME=value with a non-empty NAME and no NUL byte.");
    }
    Str *name = process_name(program);
    GC_ROOT(&program);
    io_pend(EVK_RUN, program);
    rt_check_name(EF_PROCESS, name ? name : program);
    Variant *r;
    if (rcd_mode == 2) {
        r = io_replay(EVK_RUN, program, 0, argv); /* nothing starts */
    } else {
        r = rt_run(argv, input, env, raw);
        io_record(EVK_RUN, program, 0, argv, r);
    }
    GC_UNROOT(1);
    GC_UNROOT(3);
    return r;
}
