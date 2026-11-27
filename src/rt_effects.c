/* Run-time effect enforcement and the effect builtins (research §2, §9; ported from
 * src/rt_effects.s, spec 008).
 *
 * Every effectful builtin is checked against FR_EFFECTS of the innermost frame: the effect bit
 * (defense in depth; checked programs always pass) and, for file and env effects, the scopes.
 * Paths are resolved like realpath: absolute against the start directory, `.`/`..` applied,
 * symbolic links followed (at most 40); a missing tail stays lexical. A path is allowed when it
 * lies under a resolved scope. */
#include "vo.h"

#define PATH_MAX 4096

static Str *start_cwd; /* the working directory at the first check ("" = /) */
static char res_buf[PATH_MAX + 8];
static char link_buf[PATH_MAX + 8];
static unsigned long pcg_state;
static unsigned long pcg_inc;
long opt_seed;
long opt_seed_set;
long opt_clock;
long opt_clock_set;
static long clock_next;
long clock_fixed;

/* which 1: --seed, 2: --clock-ms. */
void rt_set_option(long which, long value)
{
    if (which == 1) {
        opt_seed = value;
        opt_seed_set = 1;
    } else {
        opt_clock = value;
        opt_clock_set = 1;
    }
}

static long ent_test;
static unsigned long ent_state;

/* Seeds the generator and sets the clock for a run, or for one test (defaults: seed 1, clock 0). */
void rt_effects_start(int is_test)
{
    if (opt_clock_set) {
        clock_next = opt_clock;
        clock_fixed = 1;
    } else if (is_test) { /* tests: a fixed clock from 0 */
        clock_next = 0;
        clock_fixed = 1;
    }
    clock_stamp = clock_next; /* spec 055: log record stamps */
    long seed;
    if (opt_seed_set)
        seed = opt_seed;
    else if (is_test)
        seed = 1; /* tests: seed 1 */
    else
        seed = sys_clock_ns(); /* runs: seeded from the clock */
    pcg_seed(seed);
    ent_test = is_test; /* spec 035: tests draw reproducible "secret" bytes, from the same seed */
    ent_state = (unsigned long)seed * 0x9E3779B97F4A7C15UL + 0x243F6A8885A308D3UL;
}

/* Spec 035: random_bytes. Outside tests: the system's random source, whatever --seed says; in a test:
 * a sequence of its own from the test's seed (random_int's sequence is not disturbed). */
static unsigned long ent_next(void)
{ /* splitmix64 */
    unsigned long z = (ent_state += 0x9E3779B97F4A7C15UL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
    return z ^ (z >> 31);
}

Str *rt_entropy(long n, Node *call)
{
    if (n < 0 || n > 65536)
        rt_raise("R0007", msg("`random_bytes` needs a count from 0 to 65536, found $1.", int_to_str(n), 0, 0),
                 std_user_call(call));
    Str *b = str_new("", 0);
    if (n) {
        b = alloc_obj(OBJ_STR, n + 9);
        b->len = n;
    }
    if (ent_test) {
        for (long i = 0; i < n; i++) b->data[i] = (char)(ent_next() >> 56);
        return b;
    }
    for (long got = 0; got < n;) {
        long r = vo_syscall3(SYS_getrandom, (long)(b->data + got), n - got, 0);
        if (r == -4) continue; /* EINTR */
        if (r <= 0) /* never continue with other bytes */
            rt_raise_c("R0007", "The system's random source failed: `random_bytes` cannot continue.", std_user_call(call));
        got += r;
    }
    return b;
}

void pcg_seed(long seed)
{
    pcg_state = 0;
    pcg_inc = 109; /* (54 << 1) | 1 */
    pcg_next32();
    pcg_state += (unsigned long)seed;
    pcg_next32();
}

/* Spec 016 (research R12): a task's own sequence, from the generator's state when the call started
 * (the same for every task of a call) and the task's position path. */
void rt_task_seed(long path)
{
    ent_state ^= (unsigned long)path * 0xD6E8FEB86659FD93UL; /* spec 035: a test's tasks draw different bytes */
    pcg_state ^= (unsigned long)path * 0x9E3779B97F4A7C15UL;
    pcg_next32();
}

/* Spec 028 (research R8): a request's own sequence, from the generator's state when serving started and
 * the request's arrival number, whichever worker runs it (and in replay). */
static unsigned long pcg_serve_base;
void rt_seed_mark(void)
{
    pcg_serve_base = pcg_state;
}

void rt_request_seed(long number)
{
    pcg_state = pcg_serve_base;
    rt_task_seed(number + 1);
}

/* PCG-XSH-RR 64/32. */
long pcg_next32(void)
{
    unsigned long old = pcg_state;
    pcg_state = old * 6364136223846793005UL + pcg_inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
    unsigned rot = (unsigned)(old >> 59);
    return (long)(uint32_t)((xorshifted >> rot) | (xorshifted << ((0u - rot) & 31)));
}

/* Uniform in [lo, hi] by rejection sampling over 64-bit draws; R0007 when lo > hi. */
long rt_random_int(long lo, long hi)
{
    if (lo > hi) {
        Str *a = int_to_str(lo);
        GC_ROOT(&a);
        Str *b = int_to_str(hi);
        GC_UNROOT(1);
        rt_raise("R0007", msg("`random_int` needs lo <= hi, found $1 and $2.", a, b, 0), (void *)cur_node);
    }
    unsigned long span = (unsigned long)hi - (unsigned long)lo + 1; /* 0: the whole 64-bit range */
    unsigned long threshold = span ? (0UL - span) % span : 0;    /* (2^64 - span) mod span */
    unsigned long x;
    do {
        unsigned long high = (unsigned long)pcg_next32() << 32;
        x = high | (unsigned long)pcg_next32();
    } while (x < threshold);
    if (span) x %= span;
    return (long)((unsigned long)lo + x);
}

/* spec 022: std.time.sleep. A negative duration is R0007; replay does not wait (the readings come from
 * the recording); a fixed clock (--clock-ms, every vo test) does not wait but moves the clock on; the
 * real clock waits with ppoll until a monotonic target, never past the --timeout deadline (R0009 at
 * the user's call). Nothing is recorded: a sleep receives no data. */
void rt_sleep(long ms, Node *node)
{
    if (ms < 0) rt_raise("R0007", msg("`sleep` needs a duration of at least 0 ms, found $1.", int_to_str(ms), 0, 0),
                         std_user_call(node));
    if (rcd_mode == 2) return;
    if (clock_fixed) {
        if (clock_next > 9223372036854775807L - ms) rt_overflow(std_user_call(node));
        clock_next += ms;
        return;
    }
    out_flush(); /* spec 047: what was printed is seen before the wait, not after it */
    long now = sys_clock_ms();
    long target = ms > 9223372036854775807L - now ? 9223372036854775807L : now + ms;
    while (now < target) {
        if (deadline_ms && now >= deadline_ms) rt_timeout(std_user_call(node));
        long until = deadline_ms && deadline_ms < target ? deadline_ms : target;
        long wait = until - now > 1000 ? 1000 : until - now;
        long ts[2] = {wait / 1000, wait % 1000 * 1000000};
        vo_syscall5(SYS_ppoll, 0, 0, (long)ts, 0, 8); /* no descriptors: a timeout, or a signal */
        now = sys_clock_ms();
    }
}

long rt_now_ms(void)
{
    if (clock_fixed) return clock_stamp = clock_next++;
    return sys_clock_real_ms(); /* spec 007: wall-clock time, as documented (deadlines stay monotonic) */
}

/* result[str]: the variable's value, or err("not set"). */
Variant *rt_env(Str *name)
{
    rt_check_name(EF_ENV, name);
    char **env = (char **)envp;
    for (; env && *env; env++) {
        char *e = *env;
        if (mem_eq(e, name->data, name->len) && e[name->len] == '=')
            return make_result(0, (long)str_from_cstr(e + name->len + 1));
    }
    return make_result(1, (long)str_from_cstr("not set"));
}

/* The effect set the running code is held to. */
EffSet *rt_frame_effects(void)
{
    Frame *f = (Frame *)cur_frame;
    return f ? f->effects : 0;
}

/* ---- spec 013 (R1): every effect is checked against the running frame's grant and the root grant
 * (main's clause; a test's or property's), so no callee's own declaration, effect parameter or
 * static gap reaches outside what a person reviewed.
 * Spec 050: a target (path, host, name, address) is checked against every grant on the call path
 * that scopes its effect, not only those two: a function that declares the effect without scopes
 * (a helper or a package function that is given its target) has no limit of its own, so the scopes
 * of the functions that called it apply, each of them. ---- */

static Frame *rt_root(void)
{
    return (Frame *)rt_root_frame;
}

static int grant_has_bit(EffSet *e, long bit)
{
    return e && ((e->mask | e->any) & (unsigned long)bit); /* any-scope bits count too */
}

/* ---- spec 062: which declarations on the call path bind a target (contracts/call-path.md) ----
 * The walk reads each function's own declaration, not the frame's merged set (for a function with
 * effect parameters the merged set is the union with its caller's grant, which loses its scopes).
 * A call of a function value typed with effect parameters only (NF_FX_PARAM) is transparent: what
 * runs below it is the caller's effect parameter at work, so the caller's own scopes say nothing about
 * it, and the walk carries that on (`carried`): a generic function called by name that was handed the
 * parameter on (NF_FX_PARAM on that call) is transparent for it too, while one whose own code supplied
 * the function binds it. When the transparent call is written in an anonymous function, the activation
 * that created the anonymous function is exempt as well; it is named exactly, by a serial, because a
 * frame address alone can be a later call of the same function. */

/* Activations that can be exempt or name one: frames of named functions with effect parameters (a
 * new serial each) and frames of anonymous functions created under one (its serial). Ordered by frame
 * address; a frame being entered first drops the entries at or above it (those frames returned).
 * ponytail: 256 entries; when full a frame is not entered and nothing is exempted on its account (the
 * walk then binds more functions, never fewer). A frame field instead, if generic recursion deeper
 * than this around scoped operations ever shows in a profile. */
typedef struct FxAct {
    Frame *frame;
    Node *fn;
    long serial;
} FxAct;
#define FX_ACTS 256
static FxAct fx_acts[FX_ACTS];
long fx_nacts;
static long fx_serials;

/* serial 0: a new one (a named function with effect parameters); -1: only drop the returned frames. */
void fx_enter(Frame *f, Node *fn, long serial)
{
    while (fx_nacts && (unsigned long)fx_acts[fx_nacts - 1].frame >= (unsigned long)f) fx_nacts--;
    if (serial < 0 || fx_nacts == FX_ACTS) return;
    fx_acts[fx_nacts++] = (FxAct){f, fn, serial ? serial : ++fx_serials};
}

/* The serial f was entered with, or 0. */
long fx_serial_of(Frame *f)
{
    for (long i = fx_nacts - 1; i >= 0 && (unsigned long)fx_acts[i].frame >= (unsigned long)f; i--)
        if (fx_acts[i].frame == f && fx_acts[i].fn == f->fn) return fx_acts[i].serial;
    return 0;
}

/* The scopes f's function wrote: its declaration (a test's clause); 0 for a frame that has none
 * (anonymous functions, standard-library functions, whose declarations are cleared before a run). */
static EffSet *frame_own(Frame *f)
{
    Node *decl = f->fn;
    if (decl->kind == N_TEST) return decl->d.fx ? decl->d.fx->declared : 0;
    return decl->a.n && decl->a.n->b.fx ? decl->a.n->b.fx->declared : 0;
}

typedef struct PathIt {
    long bit;
    Frame *y;      /* the frame the walk is at */
    EffSet *set;   /* the scoped set of the frame last returned: what the target is checked against */
    long pend[8];  /* serials of exempt activations not reached yet (ponytail: 8; a ninth is not exempted) */
    int npend;
    int carried;   /* what is being checked runs as an effect parameter of the frame the walk is at */
    int root_seen; /* the root frame was judged on the way (not skipped) */
    int done;
} PathIt;

static Frame *path_next(PathIt *it);

/* The first frame, from the running one outwards, whose own declaration scopes bit; path_next the
 * following ones; after the chain the root frame (its grant) when the walk did not judge it. 0 at
 * the end. A set equal to the one last returned is not returned again (recursion).
 * ponytail: linear in the call depth for each scoped operation (calls and unscoped effects cost
 * nothing). */
static Frame *path_first(PathIt *it, long bit)
{
    *it = (PathIt){.bit = bit};
    Frame *f = (Frame *)cur_frame;
    it->y = f;
    if (!f) {
        it->done = 1;
        return 0;
    }
    if (f == rt_root()) it->root_seen = 1;
    EffSet *own = f == rt_root() ? f->effects : frame_own(f);
    /* performed by the body of a function that declares the effect only through its parameters */
    it->carried = own && es_evar_count(own) && !((own->mask | own->any) & (unsigned long)bit);
    if (own && es_scoped(own, bit)) {
        it->set = own;
        return f;
    }
    return path_next(it);
}

static Frame *path_next(PathIt *it)
{
    while (!it->done) {
        Frame *y = it->y, *p = y->parent;
        if (!p) {
            it->done = 1;
            Frame *root = rt_root();
            if (root && !it->root_seen && root->effects && root->effects != it->set && es_scoped(root->effects, it->bit)) {
                it->set = root->effects;
                return root;
            }
            return 0;
        }
        it->y = p;
        int skip = 0, carried = 0;
        Node *call = y->call;
        int marked = call && (call->kind == N_CALL || call->kind == N_CALL_VALUE) && (call->flags & NF_FX_PARAM);
        int by_name = call && call->kind == N_CALL && !call->d.i && call->c.n == y->fn;
        if (marked && !by_name) { /* a function value of p's effect parameters: p's scopes say nothing */
            skip = carried = 1;
            if (p->fn->kind == N_LAMBDA && fx_nacts) { /* written in an anonymous function: its creator is exempt */
                long serial = fx_serial_of(p);
                if (serial && it->npend < 8) it->pend[it->npend++] = serial;
            }
        } else if (marked && it->carried) { /* y was handed p's effect parameter, and that is what runs */
            skip = carried = 1;
        }
        if (it->npend && p->fn->kind != N_LAMBDA) { /* an exempt activation is reached */
            long serial = fx_serial_of(p);
            for (int i = 0; serial && i < it->npend; i++)
                if (it->pend[i] == serial) {
                    it->pend[i] = it->pend[--it->npend];
                    skip = carried = 1;
                    break;
                }
        }
        it->carried = carried;
        if (p == rt_root()) { /* the root grant always binds */
            it->root_seen = 1;
            skip = 0;
        }
        if (skip) continue;
        EffSet *own = p == rt_root() ? p->effects : frame_own(p);
        if (own && own != it->set && es_scoped(own, it->bit)) {
            it->set = own;
            return p;
        }
    }
    return 0;
}

/* 1 when the resolved path lies under one of e's scopes for bit (or bit is unscoped in e). */
static int grant_has_path(EffSet *e, long bit, Str *resolved)
{
    if (!e || !es_scoped(e, bit)) return 1;
    for (long i = 0; resolved && i < e->scopes->n; i++) {
        EffScope *sc = vec_get(e->scopes, i);
        if (sc->bit != bit) continue;
        Str *dir = rt_resolve(sc->str);
        if (dir && path_under(resolved, dir)) return 1;
    }
    return 0;
}

static void rt_denied_by(Frame *f, EffSet *declared, long bit, Str *target);
static Frame *grant_frame(Frame *f);

/* R0010 when builtin id's effect is not declared by the running function or test, or by the root. */
void rt_check_bit(long id)
{
    rt_check_effect(builtin_effects[id]);
}

void rt_check_effect(long bit)
{
    if (!bit) return;
    if (!grant_has_bit(rt_frame_effects(), bit)) rt_denied(bit, 0);
    Frame *root = rt_root();
    if (root && !grant_has_bit(root->effects, bit)) rt_denied_by(root, root->effects, bit, 0);
}

/* Scoped env (or serve, process) effects: name must be one of the declared names, in every grant on
 * the call path that scopes the effect. */
void rt_check_name(long bit, Str *name)
{
    PathIt it;
    for (Frame *f = path_first(&it, bit); f; f = path_next(&it))
        if (!es_has_pair(it.set, bit, name)) rt_denied_by(f, it.set, bit, name);
}

/* Spec 039: a connection to host:port needs a `net` scope naming the host, or the host with that port,
 * in every grant on the call path that scopes `net`. R0010 names the host, and the port too when the
 * grant has scopes with ports. */

static int grant_ports(EffSet *e)
{
    for (long i = 0; e && e->scopes && i < e->scopes->n; i++) {
        EffScope *sc = vec_get(e->scopes, i);
        if (sc->bit == EF_NET && net_scope_port(sc->str) > 0) return 1;
    }
    return 0;
}

void rt_check_net(Str *host, long port)
{
    GC_ROOT(&host);
    SB *sb = sb_new();
    sb_str(sb, host);
    sb_byte(sb, ':');
    int_to_dec(sb, port);
    Str *both = sb_finish(sb);
    GC_UNROOT(1);
    PathIt it;
    for (Frame *f = path_first(&it, EF_NET); f; f = path_next(&it))
        if (!es_pair_covered(it.set, EF_NET, both)) rt_denied_by(f, it.set, EF_NET, grant_ports(it.set) ? both : host);
}

/* Scoped file effects: the resolved path must lie under a resolved scope of every grant on the call
 * path that scopes the effect. A NUL byte
 * is refused outright (C1: the check would see the whole string, the open only its first part), and
 * so is every scoped access when the start directory is unknown (H2). */
void rt_check_path(long bit, Str *path)
{
    PathIt it;
    Frame *first = path_first(&it, bit); /* (one walk: the loop below goes on from it) */
    int scoped = first != 0;
    int nul = 0;
    for (long i = 0; i < path->len; i++) nul |= path->data[i] == 0;
    if (nul || (scoped && !rt_start_cwd_known())) rt_denied(bit, path);
    if (!scoped) return;
    GC_ROOT(&path);
    Str *resolved = rt_resolve_for(bit, path);
    GC_ROOT(&resolved);
    Frame *refused = 0;
    for (Frame *f = first; f; f = path_next(&it))
        if (!resolved || !grant_has_path(it.set, bit, resolved)) {
            refused = f; /* (it.set stays its set) */
            break;
        }
    GC_UNROOT(2);
    if (refused) rt_denied_by(refused, it.set, bit, path);
}

/* Spec 017: 1 when a grant on the call path scopes bit (operations then walk beneath the scopes, never
 * following links). */
int rt_bit_scoped(long bit)
{
    PathIt it;
    return path_first(&it, bit) != 0;
}

/* Spec 017: 1 when rt_check_path(bit, path) would pass (without raising). */
int rt_path_allowed(long bit, Str *path)
{
    for (long i = 0; i < path->len; i++)
        if (!path->data[i]) return 0;
    if (!rt_bit_scoped(bit)) return 1;
    if (!rt_start_cwd_known()) return 0;
    GC_ROOT(&path);
    Str *resolved = rt_resolve_for(bit, path);
    GC_ROOT(&resolved);
    int ok = resolved != 0;
    PathIt it;
    for (Frame *f = path_first(&it, bit); f && ok; f = path_next(&it)) ok = grant_has_path(it.set, bit, resolved);
    GC_UNROOT(2);
    return ok;
}

/* 1 when the resolved path is dir or lies inside it ("" is the root). */
int path_under(Str *path, Str *dir)
{
    long dl = dir->len;
    if (!dl) return 1;
    if ((unsigned long)path->len < (unsigned long)dl) return 0;
    if (path->len != dl && path->data[dl] != '/') return 0; /* the next byte must start a component */
    return mem_eq(path->data, dir->data, dl);
}

/* Spec 017: the path a file.delete operation acts on: the directory resolved, the last component not
 * followed (deleting or renaming a link acts on the link, wherever it points). Other bits: rt_resolve. */
Str *rt_resolve_for(long bit, Str *path)
{
    if (bit != EF_FDELETE) return rt_resolve(path);
    long cut = path->len;
    while (cut > 1 && path->data[cut - 1] == '/') cut--;
    long slash = cut;
    while (slash > 0 && path->data[slash - 1] != '/') slash--;
    long nlen = cut - slash;
    char *nm = path->data + slash;
    if (!nlen || (nlen == 1 && nm[0] == '.') || (nlen == 2 && nm[0] == '.' && nm[1] == '.')) return rt_resolve(path);
    GC_ROOT(&path);
    Str *dir = rt_resolve(slash ? str_new(path->data, slash) : str_from_cstr("."));
    Str *r = 0;
    if (dir) {
        GC_ROOT(&dir);
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_str(sb, dir);
        sb_byte(sb, '/');
        sb_bytes(sb, path->data + slash, nlen);
        r = sb_finish(sb);
        GC_UNROOT(2);
    }
    GC_UNROOT(1);
    return r;
}

/* The absolute real path as a str ("" for /), or 0 when it cannot be resolved (too long, loops).
 * ponytail: paths over 4096 bytes are refused rather than resolved */
Str *rt_resolve(Str *path)
{
    Str *rest = path; /* the remaining path (after a link, its target and the rest) */
    GC_ROOT(&rest);
    char *p = rest->data;
    long n = rest->len, i = 0;
    long len = 0;   /* resolved length (res_buf) */
    long links = 0; /* symbolic links followed */
    Str *result = 0;
    if (!n || p[0] != '/') { /* relative: start from the working directory */
        Str *cwd = rt_start_cwd();
        len = cwd->len;
        mem_copy(res_buf, cwd->data, len);
    }
    for (;;) {
        if (i >= n) {
            result = str_new(res_buf, len);
            break;
        }
        if (p[i] == '/') {
            i++;
            continue;
        }
        long from = i; /* component [from, i) */
        while (i < n && p[i] != '/') i++;
        long clen = i - from;
        if (clen == 1 && p[from] == '.') continue; /* "." */
        if (clen == 2 && p[from] == '.' && p[from + 1] == '.') { /* "..": drop the last component */
            while (len && res_buf[--len] != '/') {}
            continue;
        }
        if ((unsigned long)(len + clen) >= PATH_MAX - 2) break; /* append "/" + component */
        long before = len; /* length before the component */
        res_buf[len++] = '/';
        mem_copy(res_buf + len, p + from, clen);
        len += clen;
        res_buf[len] = 0;
        /* a symbolic link? (spec 013, L2: never looked at while replaying: the check is lexical) */
        long target = rcd_mode == 2 ? 0 : sys_readlink(res_buf, link_buf, PATH_MAX);
        if (target <= 0) continue;
        if (++links > 40) break;
        len = link_buf[0] == '/' ? 0 : before; /* replace the link with its target, then the rest */
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_bytes(sb, link_buf, target);
        sb_byte(sb, '/');
        sb_bytes(sb, p + i, n - i);
        GC_UNROOT(1);
        rest = sb_finish(sb);
        p = rest->data;
        n = rest->len;
        i = 0;
    }
    GC_UNROOT(1);
    return result;
}

static int start_cwd_failed; /* spec 013 (H2): getcwd failed: scoped file access is refused */
static long start_fd = -1;    /* spec 013 (H3): the start directory, opened once: relative paths follow it */

/* The working directory at the first call, as a str without a trailing "/" ("" for /). */
Str *rt_start_cwd(void)
{
    if (start_cwd) return start_cwd;
    long got = sys_getcwd(link_buf, PATH_MAX);
    long len = 0;
    if (got > 0) {
        len = got - 1; /* without the NUL */
        if (len == 1) len = 0; /* "/" itself */
    } else {
        start_cwd_failed = 1; /* not "/": fail closed */
    }
    start_fd = vo_syscall4(SYS_openat, AT_FDCWD, (long)".", O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
    if (start_fd < 0) start_cwd_failed = 1;
    start_cwd = str_new(link_buf, len);
    return start_cwd;
}

/* 0 when the start directory could not be determined (every scoped file access is then refused). */
int rt_start_cwd_known(void)
{
    rt_start_cwd();
    return !start_cwd_failed;
}

/* R0010: the effect (with target path or name, or 0) is outside the running declaration. */
/* Spec 009: the frame whose function's `uses` clause gave the running frame its effects. Library and
 * anonymous functions declare none at run time and run with their caller's grant (vm_push's rule),
 * so the walk goes up through them; tests and functions with a declaration stop it, as does main. */
static Frame *grant_frame(Frame *f)
{
    while (f && f->parent) {
        Node *decl = f->fn;
        EffSet *d = decl->kind == N_TEST ? 0 : decl->a.n && decl->a.n->b.fx ? decl->a.n->b.fx->declared : 0;
        /* spec 013: a declaration made only of effect parameters (`uses E`) takes its caller's grant */
        int only_params = d && !d->mask && (!d->scopes || !d->scopes->n) && es_evar_count(d);
        if (decl->kind == N_TEST || (d && !only_params)) break;
        f = f->parent;
    }
    return f;
}

void rt_denied(long bit, Str *target)
{
    Frame *gf = grant_frame((Frame *)cur_frame);
    rt_denied_by(gf, gf ? gf->effects : 0, bit, target);
}

/* A target in an R0010 message: like a scope, with control bytes as \xNN (spec 013: a path with a NUL
 * byte must not put raw bytes on the terminal). */
static void render_target(SB *sb, Str *t)
{
    static const char hex[] = "0123456789abcdef";
    sb_byte(sb, '"');
    for (long i = 0; i < t->len; i++) {
        unsigned char c = (unsigned char)t->data[i];
        if (c < 0x20 || c == 0x7f) {
            sb_cstr(sb, "\\x");
            sb_byte(sb, hex[c >> 4]);
            sb_byte(sb, hex[c & 15]);
            continue;
        }
        if (c == '"' || c == '\\') sb_byte(sb, '\\');
        sb_byte(sb, c);
    }
    sb_byte(sb, '"');
}

/* R0010 naming the grant of frame gf: the running one's (via grant_frame) or the root's. */
static void rt_denied_by(Frame *gf, EffSet *declared, long bit, Str *target)
{
    GC_ROOT(&target);
    Str *fn = frame_fn_name(gf); /* the function that declared them */
    GC_ROOT(&fn);
    SB *sb = sb_new(); /* what was attempted: effect("target") */
    GC_ROOT(&sb); /* (the same root covers the second builder below) */
    char *effect = es_bit_name(bit);
    sb_cstr(sb, effect);
    if (target) {
        sb_byte(sb, '(');
        render_target(sb, target);
        sb_byte(sb, ')');
    }
    Str *what = sb_finish(sb);
    GC_ROOT(&what);
    Str *decl_text = str_from_cstr("nothing (pure)"); /* what is declared */
    GC_ROOT(&decl_text);
    if (declared && !es_is_empty(declared)) decl_text = fx_render(declared);
    Str *m = msg("$1 is not allowed: `$2` declares $3.", what, fn, decl_text);
    GC_ROOT(&m);
    sb = sb_new(); /* JSON fields for the report */
    sb_cstr(sb, ",\"effect\":\"");
    sb_cstr(sb, effect);
    sb_cstr(sb, "\",\"target\":");
    if (target)
        json_str_into(sb, target);
    else
        sb_cstr(sb, "null");
    sb_cstr(sb, ",\"allowed\":[");
    Vec *scopes = declared ? declared->scopes : 0;
    int first = 1;
    for (long i = 0; scopes && i < scopes->n; i++) {
        EffScope *sc = vec_get(scopes, i);
        if (sc->bit != bit) continue;
        if (!first) sb_byte(sb, ',');
        first = 0;
        SB *one = sb_new();
        GC_ROOT(&one);
        es_render_scope(one, sc->str);
        GC_UNROOT(1);
        Str *text = sb_finish(one);
        sb_bytes(sb, text->data, text->len);
    }
    sb_cstr(sb, "],\"declared_by\":");
    json_str_into(sb, fn);
    rt_err_extra = (long)sb_finish(sb);
    GC_UNROOT(6);
    rt_raise("R0010", m, (void *)cur_node);
}

/* ---- spec 013 (R2, H3, H4): opening a file that passed rt_check_path ----
 * Unscoped grants open from the start directory's descriptor. Scoped grants open the resolved path
 * relative to the outermost granted scope directory that holds it (opened from the start descriptor, so
 * a renamed working directory changes nothing), one component at a time with O_NOFOLLOW: whatever links
 * change after the check, the open cannot leave that directory; an attempt is R0010. Works on every
 * kernel (research R3's fallback; openat2 is missing under qemu user mode). The fd, or a negative errno. */
#define E_LOOP 40

/* Spec 017: the parent directory of a path that passed rt_check_path, as a descriptor (closed by the
 * caller), and the last component in *name ("." for the scope directory itself). Scoped grants walk
 * from the outermost granted scope directory as rt_open_file does; with strict, a scope directory
 * itself is refused (R0010: delete and rename never remove a scope). Unscoped: the path's directory,
 * opened from the start directory. A negative errno on failure. */
static long open_parent(long bit, Str *path, Str **name, int strict, int scoped);

long rt_open_parent(long bit, Str *path, Str **name, int strict)
{
    return open_parent(bit, path, name, strict, rt_bit_scoped(bit));
}

/* (scoped: a declaration on the call path scopes bit) */
static long open_parent(long bit, Str *path, Str **name, int strict, int scoped)
{
    rt_start_cwd();
    long base = start_fd >= 0 ? start_fd : AT_FDCWD;
    GC_ROOT(&path);
    if (!scoped) {
        long cut = path->len;
        while (cut > 0 && path->data[cut - 1] == '/') cut--; /* "a/b/" names b */
        long slash = cut;
        while (slash > 0 && path->data[slash - 1] != '/') slash--;
        *name = slash == cut ? str_from_cstr(".") : str_new(path->data + slash, cut - slash);
        Str *dir = slash ? str_new(path->data, slash) : str_from_cstr(!cut && path->len ? "/" : "."); /* "/" is the root */
        GC_UNROOT(1);
        return vo_syscall4(SYS_openat, base, (long)dir->data, O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
    }
    Str *resolved = rt_resolve_for(bit, path);
    GC_ROOT(&resolved);
    /* The outermost granted scope holding the path, and its resolved form: the scopes nested in it are
       then walked without following links like every other component (opened by its text, a nested
       scope swapped for a link after the check led outside the outer grant). */
    Str *scope = 0, *dir = 0;
    long deepest = 0; /* the length of the deepest one's resolved form */
    GC_ROOT(&scope);
    GC_ROOT(&dir);
    int every = 1; /* every scoped grant on the call path still holds it, as when it was checked */
    PathIt it;
    for (Frame *f = resolved ? path_first(&it, bit) : 0; f; f = path_next(&it)) {
        int held = 0;
        for (long i = 0; i < it.set->scopes->n; i++) {
            EffScope *sc = vec_get(it.set->scopes, i);
            if (sc->bit != bit) continue;
            Str *d = rt_resolve(sc->str);
            if (!d || !path_under(resolved, d)) continue;
            held = 1;
            if (d->len > deepest) deepest = d->len;
            if (!dir || d->len < dir->len) {
                dir = d;
                scope = sc->str;
            }
        }
        if (!held) every = 0;
    }
    if (!dir || !every || (strict && resolved->len == deepest)) { /* changed since the check, or the scope itself */
        GC_UNROOT(4);
        rt_denied(bit, path);
    }
    long fd = vo_syscall4(SYS_openat, base, (long)(scope->len ? scope->data : "."), O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
    /* the resolved path below the scope (no links, no ".."), one component at a time without following
       links: a component swapped for a link after the check fails the open instead of escaping (H4) */
    char *rel = resolved->data + dir->len, *end = resolved->data + resolved->len;
    *name = str_from_cstr(".");
    while (fd >= 0) {
        while (rel < end && *rel == '/') rel++;
        if (rel >= end) break;
        char *c = rel;
        while (rel < end && *rel != '/') rel++;
        if (rel >= end) { /* the last component: its parent is fd */
            *name = str_new(c, rel - c);
            break;
        }
        char save = *rel;
        *rel = 0;
        long next = vo_syscall4(SYS_openat, fd, (long)c, O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC, 0);
        *rel = save;
        sys_close((int)fd);
        fd = next;
    }
    GC_UNROOT(4);
    if (fd == -E_LOOP) rt_denied(bit, path); /* a link where the check saw none */
    return fd;
}

long rt_open_file(long bit, Str *path, long flags)
{
    rt_start_cwd();
    long base = start_fd >= 0 ? start_fd : AT_FDCWD;
    int scoped = rt_bit_scoped(bit);
    if (!scoped) return vo_syscall4(SYS_openat, base, (long)path->data, flags | O_CLOEXEC, 0644);
    Str *name = 0;
    GC_ROOT(&name);
    long parent = open_parent(bit, path, &name, 0, scoped);
    long r = parent;
    if (parent >= 0) {
        r = vo_syscall4(SYS_openat, parent, (long)name->data, flags | O_NOFOLLOW | O_CLOEXEC, 0644);
        sys_close((int)parent);
    }
    GC_UNROOT(1);
    if (r == -E_LOOP) rt_denied(bit, path); /* a link where the check saw none */
    return r;
}
