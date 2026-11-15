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
    long seed;
    if (opt_seed_set)
        seed = opt_seed;
    else if (is_test)
        seed = 1; /* tests: seed 1 */
    else
        seed = sys_clock_ns(); /* runs: seeded from the clock */
    pcg_seed(seed);
}

void pcg_seed(long seed)
{
    pcg_state = 0;
    pcg_inc = 109; /* (54 << 1) | 1 */
    pcg_next32();
    pcg_state += (unsigned long)seed;
    pcg_next32();
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
        rt_raise("R0007", msg("`random_int` needs lo <= hi, got $1 and $2.", a, b, 0), (void *)cur_node);
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

long rt_now_ms(void)
{
    if (clock_fixed) return clock_next++;
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

/* R0010 when builtin id's effect is not declared by the running function or test. */
void rt_check_bit(long id)
{
    long bit = builtin_effects[id];
    if (!bit) return;
    EffSet *eff = rt_frame_effects();
    if (eff && ((eff->mask | eff->any) & (unsigned long)bit)) return; /* any-scope bits count too */
    rt_denied(bit, 0);
}

/* Scoped env (or net) effects: name must be one of the declared names. */
void rt_check_name(long bit, Str *name)
{
    EffSet *eff = rt_frame_effects();
    if (!eff || !es_scoped(eff, bit) || es_has_pair(eff, bit, name)) return;
    rt_denied(bit, name);
}

/* Scoped file effects: the resolved path must lie under a resolved scope. */
void rt_check_path(long bit, Str *path)
{
    EffSet *eff = rt_frame_effects();
    if (!eff || !es_scoped(eff, bit)) return;
    GC_ROOT(&path);
    Str *resolved = rt_resolve(path);
    GC_ROOT(&resolved);
    int ok = 0;
    for (long i = 0; resolved && !ok && i < eff->scopes->n; i++) {
        EffScope *sc = vec_get(eff->scopes, i);
        if (sc->bit != bit) continue;
        Str *dir = rt_resolve(sc->str);
        ok = dir && path_under(resolved, dir);
    }
    GC_UNROOT(2);
    if (!ok) rt_denied(bit, path);
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
        long target = sys_readlink(res_buf, link_buf, PATH_MAX); /* a symbolic link? */
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

/* The working directory at the first call, as a str without a trailing "/" ("" for /). */
Str *rt_start_cwd(void)
{
    if (start_cwd) return start_cwd;
    long got = sys_getcwd(link_buf, PATH_MAX);
    long len = 0;
    if (got > 0) {
        len = got - 1; /* without the NUL */
        if (len == 1) len = 0; /* "/" itself */
    }
    start_cwd = str_new(link_buf, len);
    return start_cwd;
}

/* R0010: the effect (with target path or name, or 0) is outside the running declaration. */
/* Spec 009: the frame whose function's `uses` clause gave the running frame its effects. Library and
 * anonymous functions declare none at run time and run with their caller's grant (vm_push's rule),
 * so the walk goes up through them; tests and functions with a declaration stop it, as does main. */
static Frame *grant_frame(Frame *f)
{
    while (f && f->parent) {
        Node *decl = f->fn;
        if (decl->kind == N_TEST || (decl->a.n && decl->a.n->b.fx && decl->a.n->b.fx->declared)) break;
        f = f->parent;
    }
    return f;
}

void rt_denied(long bit, Str *target)
{
    EffSet *declared = rt_frame_effects(); /* (may be 0) */
    GC_ROOT(&target);
    Str *fn = frame_fn_name(grant_frame((Frame *)cur_frame)); /* the function that declared them */
    GC_ROOT(&fn);
    SB *sb = sb_new(); /* what was attempted: effect("target") */
    GC_ROOT(&sb); /* (the same root covers the second builder below) */
    char *effect = es_bit_name(bit);
    sb_cstr(sb, effect);
    if (target) {
        sb_byte(sb, '(');
        es_render_scope(sb, target);
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
