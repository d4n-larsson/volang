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
#define E_NOTDIR 20
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
/* Spec 057: "./", the start directory: always allowed (as for run), nothing opened, not recorded. */
static int start_dir(Str *dir)
{
    return dir->len == 2 && dir->data[0] == '.' && dir->data[1] == '/';
}

/* Spec 057 (R1, R3): the checked working directory opened for the child's fchdir, without following
 * links where a scope limits it (as std.fs reaches a checked path); -1 for the start directory. On
 * failure, a negative value and *bad the result. */
static long open_dir(Str *dir, Variant **bad)
{
    if (start_dir(dir)) return -1;
    Str *name = 0;
    GC_ROOT(&dir);
    GC_ROOT(&name);
    long fd = rt_open_parent(EF_FREAD, dir, &name, 0);
    if (fd >= 0 && !(name->len == 1 && name->data[0] == '.')) {
        long flags = O_PATH | O_DIRECTORY | O_CLOEXEC | (rt_bit_scoped(EF_FREAD) ? O_NOFOLLOW : 0);
        long sub = vo_syscall4(SYS_openat, fd, (long)name->data, flags, 0);
        sys_close((int)fd);
        fd = sub;
    }
    GC_UNROOT(2);
    if (fd < 0)
        *bad = make_err(fd == -E_NOENT    ? "directory not found"
                        : fd == -E_NOTDIR ? "not a directory"
                                          : "directory not accessible");
    return fd;
}

/* Spec 057 (R2): the file to execute, after the child entered dir: a relative path with "/" is the start
 * directory's file, as without a directory. 0 when the start directory is unknown. */
static Str *exec_path(Str *path, long dir_fd)
{
    if (dir_fd < 0 || path->data[0] == '/') return path;
    if (!rt_start_cwd_known()) return 0;
    GC_ROOT(&path);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_str(sb, rt_start_cwd());
    sb_byte(sb, '/');
    sb_str(sb, path);
    GC_UNROOT(2);
    return sb_finish(sb);
}

/* Spec 057: a child's fchdir failure, sent on the error pipe above any errno of execve. */
#define DIR_FAILED 1000

static void child(int in, int out, int err, int ex, char *path, char **argv, char **envv, long parent, int dir)
{
    if (dir >= 0) { /* spec 057: first, before 0 to 2 are replaced (dir may be one of them) */
        long r = vo_syscall1(SYS_fchdir, dir);
        if (r < 0) {
            int code = DIR_FAILED + (int)-r;
            sys_write(ex, &code, sizeof code);
            vo_syscall1(SYS_exit_group, 127);
        }
    }
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

/* Runs a checked program, in the directory dir_fd when it is not -1 (spec 057); the result text or err. */
static Variant *rt_run(List *argv, Str *input, List *env, int raw, long dir_fd)
{
    Str *program = (Str *)argv->data[0].p;
    GC_ROOT(&argv);
    GC_ROOT(&input);
    GC_ROOT(&env);
    Str *path = resolve_program(program);
    GC_ROOT(&path);
    if (path) path = exec_path(path, dir_fd);
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
    if (pid == 0) child(pin[0], pout[1], perr[1], pex[1], path->data, av, ev, parent, (int)dir_fd);
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
    } else if (failed >= DIR_FAILED) { /* spec 057: the child could not enter the directory */
        res = make_result(1, (long)str_from_cstr("directory not accessible"));
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

/* The argument checks (R0007) and the program against both grants (R0010) for a run of argv with
 * env: the program as given. Spec 057: then dir against the file.read grant (R0010), unless "./". */
static Str *run_check(List *argv, List *env, Str *dir)
{
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
    if (!dir->len || has_nul(dir)) raise_arg("A directory must be a non-empty path without NUL bytes.");
    Str *name = process_name(program);
    io_pend(EVK_RUN, program);
    rt_check_name(EF_PROCESS, name ? name : program);
    if (!start_dir(dir)) rt_check_path(EF_FREAD, dir);
    return program;
}

/* Spec 057 (R4): the builtins' first argument is [dir, program, args...]: dir, and the run's argv. */
static List *split_dir(List *all, Str **dir)
{
    if (!all->len) raise_arg("A program to run is needed.");
    *dir = (Str *)all->data[0].p;
    return list_slice(all, 1, all->len, (Node *)cur_node);
}

/* Checks the arguments (R0007) and the program against both grants (R0010), then runs it. raw (spec
 * 021, run_bytes): the output is returned as it is, not checked as UTF-8. Spec 057: all is
 * [dir, program, args...]; the run is recorded with dir unless it is "./". */
Variant *io_run(List *all, Str *input, List *env, int raw)
{
    Str *dir = 0;
    GC_ROOT(&input);
    GC_ROOT(&env);
    GC_ROOT(&dir);
    List *argv = split_dir(all, &dir);
    GC_ROOT(&argv);
    Str *program = run_check(argv, env, dir);
    GC_ROOT(&program);
    Str *to = start_dir(dir) ? 0 : dir;
    Variant *r = 0;
    GC_ROOT(&r);
    if (rcd_mode == 2) {
        r = io_replay(EVK_RUN, program, to, argv); /* nothing starts */
    } else {
        long fd = open_dir(dir, &r);
        if (fd >= 0 || start_dir(dir)) r = rt_run(argv, input, env, raw, fd);
        if (fd >= 0) sys_close((int)fd);
        io_record(EVK_RUN, program, to, argv, r);
    }
    GC_UNROOT(6);
    return r;
}

/* ---- spec 040: a followed child (std.process.run_lines) ----
 * `__proc_start` starts a program as `run_with` does and keeps it in a table; `__proc(1, id)` returns
 * its next line of standard output as soon as it is complete, feeding its input and collecting its
 * standard error meanwhile; `__proc(2, id)` waits for it to end after its output ended, and
 * `__proc(3, id)` kills it (the passed function stopped the stream). Both return "CODE\nSTDERR".
 * The loop that calls these is vo code (std/process.vo). proc_sweep ends what a stopped program or
 * test left running. Buffers are mapped: nothing here is in the collected heap or large and static.
 * ponytail: 16 followed children at a time; no time limit per line (the run's deadline only). */

#define PROC_MAX 16
#define LINE_MAX (16L << 20)

typedef struct MBuf {
    char *p;
    long len, cap;
} MBuf;

typedef struct Proc {
    int used;
    long pid;   /* 0 in a replay */
    long owner; /* the process that started it: a task's copy of the table owns nothing */
    int in, out, err; /* the parent's pipe ends; -1 when closed */
    MBuf input; /* what is still to be written starts at sent */
    long sent;
    MBuf line; /* standard output not yet delivered starts at pos */
    long pos;
    MBuf errb; /* standard error so far */
    int over;  /* standard error passed the limit */
    char name[256]; /* the program as given: the name in recordings */
} Proc;

static Proc procs[PROC_MAX];

/* Room for need more bytes; 0 when memory cannot be mapped. */
static int mbuf_room(MBuf *b, long need)
{
    if (b->cap - b->len >= need) return 1;
    long cap = b->cap ? b->cap * 2 : 65536;
    while (cap - b->len < need) cap *= 2;
    long p = vo_syscall6(SYS_mmap, 0, cap, 3 /* read, write */, 0x4022 /* private, anonymous, no reserve */, -1, 0);
    if (p < 0 && p > -4096) return 0;
    mem_copy((char *)p, b->p, b->len);
    if (b->p) vo_syscall2(SYS_munmap, (long)b->p, b->cap);
    b->p = (char *)p;
    b->cap = cap;
    return 1;
}

static void mbuf_free(MBuf *b)
{
    if (b->p) vo_syscall2(SYS_munmap, (long)b->p, b->cap);
    *b = (MBuf){0, 0, 0};
}

static void proc_free(Proc *p)
{
    close_fd(&p->in), close_fd(&p->out), close_fd(&p->err);
    mbuf_free(&p->input), mbuf_free(&p->line), mbuf_free(&p->errb);
    p->used = 0;
}

/* Kills the child's group and reaps it: its wait status. */
static int proc_kill(Proc *p)
{
    int status = 0;
    vo_syscall2(SYS_kill, -p->pid, SIGKILL);
    vo_syscall2(SYS_kill, p->pid, SIGKILL);
    sys_wait(p->pid, &status);
    return status;
}

/* Ends every followed child this process started: at exit and after each test, so a stream that a
 * runtime error or `exit` cut short leaves nothing running. */
void proc_sweep(void)
{
    long me = 0;
    for (int i = 0; i < PROC_MAX; i++) {
        Proc *p = &procs[i];
        if (!p->used) continue;
        if (!me) me = sys_getpid();
        if (p->pid > 0 && p->owner == me) proc_kill(p);
        if (p->owner == me || !p->pid) proc_free(p);
    }
}

/* Reads what the pipe has into b (up to limit bytes in all); the pipe is closed at its end. */
static void proc_read(int *fd, MBuf *b, long limit, int *over)
{
    while (*fd >= 0) {
        if (b->len >= limit) {
            if (over) *over = 1;
            char drop[4096]; /* past the limit: read and drop, so the child is not left blocked */
            long n = over ? sys_read(*fd, drop, sizeof drop) : -E_AGAIN;
            if (n == -E_AGAIN || !over) return;
            if (n == -E_INTR) continue;
            if (n <= 0) close_fd(fd);
            continue;
        }
        if (!mbuf_room(b, 65536)) return;
        long want = b->cap - b->len < limit - b->len ? b->cap - b->len : limit - b->len;
        long n = sys_read(*fd, b->p + b->len, want);
        if (n == -E_AGAIN) return;
        if (n == -E_INTR) continue;
        if (n <= 0) {
            close_fd(fd);
            return;
        }
        b->len += n;
    }
}

/* Waits (at most tick ms; -1: until something happens) for the child's pipes and moves what it can:
 * standard output when out is set, standard error, the input. R0009 past the run's deadline. */
static void proc_pump(Proc *p, int out, long tick)
{
    PollFd fds[3];
    int nf = 0;
    if (out && p->out >= 0) fds[nf++] = (PollFd){p->out, POLLIN, 0};
    if (p->err >= 0) fds[nf++] = (PollFd){p->err, POLLIN, 0};
    if (p->in >= 0) fds[nf++] = (PollFd){p->in, POLLOUT, 0};
    long wait = tick;
    if (deadline_ms) {
        long left = deadline_ms - sys_clock_ms();
        if (left <= 0) rt_timeout((Node *)cur_node); /* (the sweep ends the child) */
        if (wait < 0 || left < wait) wait = left;
    }
    long ts[2] = {wait / 1000, (wait % 1000) * 1000000};
    vo_syscall5(SYS_ppoll, (long)fds, nf, wait < 0 ? 0 : (long)ts, 0, 8);
    if (out) proc_read(&p->out, &p->line, LINE_MAX + 65536, 0);
    proc_read(&p->err, &p->errb, MAX_OUTPUT, &p->over);
    if (p->in >= 0) { /* a child that stopped reading gives EPIPE, with SIGPIPE held back and dropped */
        unsigned long pipe_sig = 1UL << (SIGPIPE - 1), old = 0;
        vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&pipe_sig, (long)&old, 8);
        long w = sys_write(p->in, p->input.p + p->sent, p->input.len - p->sent);
        if (w > 0) p->sent += w;
        if ((w < 0 && w != -E_AGAIN && w != -E_INTR) || p->sent >= p->input.len) close_fd(&p->in);
        int s2 = (int)vo_syscall4(SYS_signalfd4, -1, (long)&pipe_sig, 8, O_NONBLOCK | O_CLOEXEC);
        char info[128];
        while (s2 >= 0 && sys_read(s2, info, sizeof info) > 0) {}
        if (s2 >= 0) sys_close(s2);
        vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&old, 0, 8);
    }
}

/* __proc_start([dir, program, args...], input, env) -> result[int] (spec 057: dir as for io_run) */
Variant *io_proc_start(List *all, Str *input, List *env)
{
    Str *dir = 0;
    GC_ROOT(&input);
    GC_ROOT(&env);
    GC_ROOT(&dir);
    List *argv = split_dir(all, &dir);
    GC_ROOT(&argv);
    Str *program = run_check(argv, env, dir);
    GC_ROOT(&program);
    Str *to = start_dir(dir) ? 0 : dir;
    GC_ROOT(&to);
    long fd = -1;
    Variant *r = 0;
    GC_ROOT(&r);
    Proc *p = 0;
    for (int i = 0; i < PROC_MAX && !p; i++)
        if (!procs[i].used) p = &procs[i];
    Str *path = 0;
    GC_ROOT(&path);
    if (rcd_mode == 2) {
        r = io_replay(EVK_RUN, program, to, argv);
        if (r->tag == 0 && !p) r = make_err("too many programs are being followed");
    } else if (!p) {
        r = make_err("too many programs are being followed");
    } else if ((fd = open_dir(dir, &r)) < 0 && !start_dir(dir)) {
        /* r: the directory's error */
    } else if (!(path = resolve_program(program)) || !(path = exec_path(path, fd))) {
        r = make_err("not found");
    }
    if (r && (r->tag || !p)) {
        if (fd >= 0) sys_close((int)fd);
        if (rcd_mode != 2) io_record(EVK_RUN, program, to, argv, r);
        GC_UNROOT(8);
        return r;
    }
    *p = (Proc){0};
    p->in = p->out = p->err = -1;
    long n = program->len < 255 ? program->len : 255;
    mem_copy(p->name, program->data, n);
    if (rcd_mode == 2) { /* nothing starts: the lines come from the recording */
        p->used = 1;
        GC_UNROOT(8);
        return make_result(0, p - procs);
    }
    char **av = alloc_bytes((argv->len + 1) * 8);
    GC_ROOT(&av);
    char **ev = alloc_bytes((env->len + 1) * 8);
    GC_ROOT(&ev);
    for (long i = 0; i < argv->len; i++) av[i] = ((Str *)argv->data[i].p)->data;
    for (long i = 0; i < env->len; i++) ev[i] = ((Str *)env->data[i].p)->data;
    int pin[2] = {-1, -1}, pout[2] = {-1, -1}, perr[2] = {-1, -1}, pex[2] = {-1, -1};
    long pid = -1;
    int failed = 0;
    if (!mbuf_room(&p->input, input->len + 1) || sys_pipe(pin) < 0 || sys_pipe(pout) < 0 || sys_pipe(perr) < 0 ||
        sys_pipe(pex) < 0) {
        close_fd(&pin[0]), close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&pout[1]);
        close_fd(&perr[0]), close_fd(&perr[1]), close_fd(&pex[0]), close_fd(&pex[1]);
        r = make_err("cannot run: no pipes");
    } else { /* (as rt_run starts its child) */
        long parent = sys_getpid();
        pid = sys_fork();
        if (pid == 0) child(pin[0], pout[1], perr[1], pex[1], path->data, av, ev, parent, (int)fd);
        close_fd(&pin[0]), close_fd(&pout[1]), close_fd(&perr[1]), close_fd(&pex[1]);
        if (pid > 0) {
            int code = 0;
            long got;
            do {
                got = sys_read(pex[0], &code, sizeof code);
            } while (got == -E_INTR);
            if (got == sizeof code) failed = code ? code : 1;
        }
        close_fd(&pex[0]);
        if (pid < 0 || failed) {
            int status = 0;
            if (pid > 0) sys_wait(pid, &status);
            close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&perr[0]);
            r = pid < 0                 ? make_result(1, (long)msg("cannot run: error $1", int_to_str(-pid), 0, 0))
                : failed >= DIR_FAILED  ? make_err("directory not accessible") /* spec 057 */
                : failed == E_NOENT ? make_err("not found")
                : failed == E_ACCES ? make_err("permission denied")
                                    : make_result(1, (long)msg("cannot run: error $1", int_to_str(failed), 0, 0));
        }
    }
    if (fd >= 0) sys_close((int)fd);
    if (r) {
        mbuf_free(&p->input);
        io_record(EVK_RUN, program, to, argv, r);
        GC_UNROOT(10);
        return r;
    }
    set_flags(pin[1], O_NONBLOCK);
    set_flags(pout[0], O_NONBLOCK);
    set_flags(perr[0], O_NONBLOCK);
    p->used = 1;
    p->pid = pid;
    p->owner = sys_getpid();
    p->in = pin[1], p->out = pout[0], p->err = perr[0];
    mem_copy(p->input.p, input->data, input->len);
    p->input.len = input->len;
    if (!input->len) close_fd(&p->in);
    r = make_result(0, (long)str_new("", 0));
    io_record(EVK_RUN, program, to, argv, r);
    GC_UNROOT(10);
    return make_result(0, p - procs);
}

/* The next line of standard output (without `\n` or `\r\n`), err("eof") at its end, or err for a
 * line that is not UTF-8 or too long. */
static Variant *proc_line(Proc *p)
{
    long looked = 0;
    for (;;) {
        char *s = p->line.p + p->pos;
        long have = p->line.len - p->pos, n = -1;
        for (; looked < have; looked++)
            if (s[looked] == '\n') {
                n = looked;
                break;
            }
        long used = n + 1;
        if (n < 0 && p->out < 0) { /* the output ended: what is left is the last line */
            if (!have) return make_err("eof");
            n = used = have;
        }
        if (n >= 0) {
            if (n && used > n && s[n - 1] == '\r') n--;
            Variant *r = n > LINE_MAX                  ? make_err("longer than 16 MiB")
                         : utf8_validate(s, n) >= 0 ? make_err("not UTF-8")
                                                    : make_result(0, (long)str_new(s, n));
            p->pos += used;
            if (p->pos == p->line.len) p->pos = p->line.len = 0;
            return r;
        }
        if (have > LINE_MAX) return make_err("longer than 16 MiB");
        if (p->pos) { /* slide what is left to the start before reading more */
            for (long i = 0; i < have; i++) p->line.p[i] = s[i];
            p->line.len = have;
            p->pos = 0;
        }
        proc_pump(p, 1, -1);
    }
}

/* "CODE\nSTDERR" for a child that ended with wait status `status`, or err as `run` gives. */
static Variant *proc_result(Proc *p, int status)
{
    if (p->over) return make_err("output is larger than 16 MiB");
    if (utf8_validate(p->errb.p, p->errb.len) >= 0) return make_err("output is not UTF-8 (run_bytes reads it)");
    SB *sb = sb_new();
    int_to_dec(sb, (status & 0x7f) ? 128 + (status & 0x7f) : (status >> 8) & 0xff);
    sb_byte(sb, '\n');
    sb_bytes(sb, p->errb.p, p->errb.len);
    return make_result(0, (long)sb_finish(sb));
}

/* __proc(op, id) -> result[str] */
Variant *io_proc(long op, long id)
{
    if (id < 0 || id >= PROC_MAX || !procs[id].used || op < 1 || op > 3) return make_err("not running");
    Proc *p = &procs[id];
    Str *name = str_from_cstr(p->name);
    GC_ROOT(&name);
    long kind = op == 1 ? EVK_PROC_LINE : EVK_PROC_END;
    Variant *r;
    if (rcd_mode == 2) {
        r = rp_result(rp_take(kind, name));
    } else if (op == 1) {
        r = proc_line(p);
    } else {
        int status = 0;
        if (op == 3) {
            status = proc_kill(p);
        } else { /* the output ended: wait as `run` does, still feeding input and collecting stderr */
            close_fd(&p->out);
            while (vo_syscall4(SYS_wait4, p->pid, (long)&status, WNOHANG, 0) != p->pid) proc_pump(p, 0, 20);
            vo_syscall2(SYS_kill, -p->pid, SIGKILL); /* the child's group: whatever it started */
        }
        p->pid = 0;
        proc_read(&p->err, &p->errb, MAX_OUTPUT, &p->over);
        r = proc_result(p, status);
    }
    if (rcd_mode == 1) {
        GC_ROOT(&r);
        rcd_event(kind, name, r->tag ? EVR_ERR : EVR_OK, r->pay.s, 0);
        GC_UNROOT(1);
    }
    if (op != 1) proc_free(p);
    GC_UNROOT(1);
    return r;
}
