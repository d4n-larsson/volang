/* Effect sets with scopes (data-model.md "Effect set", research §3; ported from src/effects.s,
 * spec 008).
 *
 * A set is ES_MASK (bits declared, unscoped unless the set has scope pairs for the bit), ES_SCOPES
 * (vec of {bit, str} pairs, sorted by bit then bytes, no duplicates) and ES_ANY (bits needed "in some
 * scope", from builtin uses; never part of a declaration). ES_ANY may overlap ES_MASK: then the scopes
 * are still the requirement, and none counts as unused. Sets are built with es_new / es_add_scope /
 * es_put_pair and not changed afterwards. Directory scopes are normalized: no leading "./", no empty
 * or "." components, one trailing "/"; "" stands for the working directory itself and renders as
 * "./". */
#include "vo.h"

static const char *const es_names[] = {"console", "input",  "file.read", "file.write", "file.delete", "net",
                                       "clock",   "random", "env",       "serve",      "process",     "entropy", 0};

EffSet *es_new(long mask)
{
    EffSet *s = alloc(ES_SIZE);
    s->mask = (unsigned long)mask;
    return s;
}

char *es_bit_name(long bit)
{
    int i = 0;
    for (unsigned long b = (unsigned long)bit >> 1; b; b >>= 1) i++;
    return (char *)es_names[i];
}

/* The effect bit named by the n bytes at p, or 0. */
long es_bit_by_name(char *p, long n)
{
    for (int i = 0; es_names[i]; i++)
        if (cstr_len((char *)es_names[i]) == n && mem_eq(p, (char *)es_names[i], n)) return 1L << i;
    return 0;
}

/* 1 when the set has at least one scope pair for bit. */
int es_scoped(EffSet *set, long bit)
{
    Vec *v = set->scopes;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++)
        if (((EffScope *)v->data[i].p)->bit == bit) return 1;
    return 0;
}

/* Appends a new {bit, str} pair to vec. */
static void es_pair_push(Vec *v, long bit, Str *str)
{
    GC_ROOT(&v);
    GC_ROOT(&str);
    EffScope *p = alloc(ESC_SIZE);
    p->bit = bit;
    p->str = str;
    vec_push(v, p);
    GC_UNROOT(2);
}

/* Sets bit and inserts the scope pair in (bit, bytes) order unless it is already present. */
void es_put_pair(EffSet *set, long bit, Str *str)
{
    GC_ROOT(&set);
    GC_ROOT(&str);
    set->mask |= (unsigned long)bit;
    Vec *pairs = vec_new(); /* the new pair vector */
    GC_ROOT(&pairs);
    Vec *old = set->scopes;
    GC_ROOT(&old);
    int placed = 0;
    for (long i = 0; old && i < old->n; i++) {
        EffScope *p = vec_get(old, i);
        if (!placed && (unsigned long)p->bit >= (unsigned long)bit) {
            if ((unsigned long)bit < (unsigned long)p->bit) {
                es_pair_push(pairs, bit, str); /* place the new pair before this one */
                placed = 1;
            } else {
                int c = str_cmp(p->str, str);
                if (c > 0) {
                    es_pair_push(pairs, bit, str);
                    placed = 1;
                } else if (c == 0) {
                    placed = 1; /* already present: keep the old pair */
                }
            }
        }
        vec_push(pairs, p);
    }
    if (!placed) es_pair_push(pairs, bit, str);
    set->scopes = pairs;
    GC_UNROOT(4);
}

/* The port of a serve address, "PORT" (loopback) or "0.0.0.0:PORT" (all interfaces), 0-65535 (spec
 * 039: 0 is a port the system picks, for std.tcp); -1 when the text is neither. *all is set to 1 for
 * the second form (when all is given). */
long serve_address_parse(Str *s, long *all)
{
    long i = 0;
    if (s->len > 8 && mem_eq(s->data, "0.0.0.0:", 8)) i = 8;
    if (all) *all = i != 0;
    if (i == s->len || s->len - i > 5 || (s->data[i] == '0' && s->len - i > 1)) return -1;
    long port = 0;
    for (; i < s->len; i++) {
        if (s->data[i] < '0' || s->data[i] > '9') return -1;
        port = port * 10 + (s->data[i] - '0');
    }
    return port <= 65535 ? port : -1;
}

/* The same for std.http.serve, which needs a port of its own: 1-65535, 0 otherwise. */
long serve_address_port(Str *s, long *all)
{
    long port = serve_address_parse(s, all);
    return port > 0 ? port : 0;
}

/* Spec 039: where the port of a `net` scope "HOST:PORT" starts (after the colon), or 0 for a bare
 * host. -1 when there is a colon without a host before it or a port 1-65535 (no leading zero) after. */
long net_scope_port(Str *s)
{
    long c = -1;
    for (long i = 0; i < s->len && c < 0; i++)
        if (s->data[i] == ':') c = i;
    if (c < 0) return 0;
    long n = s->len - c - 1, port = 0;
    if (c == 0 || n < 1 || n > 5 || s->data[c + 1] == '0') return -1;
    for (long i = c + 1; i < s->len; i++) {
        if (s->data[i] < '0' || s->data[i] > '9') return -1;
        port = port * 10 + (s->data[i] - '0');
    }
    return port <= 65535 ? c + 1 : -1;
}

/* Spec 017: a program as a `process` scope or a program a run names: a bare name, or a relative or
 * absolute path without a ".." component, a trailing "/" or a NUL byte; 0 otherwise. A leading "./"
 * is dropped while a "/" remains ("./bin/x" is "bin/x"; "./x", a file here, stays apart from "x",
 * which is looked up through PATH). */
Str *process_name(Str *s)
{
    long i = 0;
    for (;;) {
        if (s->len - i < 2 || s->data[i] != '.' || s->data[i + 1] != '/') break;
        int slash = 0;
        for (long k = i + 2; k < s->len; k++) slash |= s->data[k] == '/';
        if (!slash) break;
        i += 2;
    }
    long n = s->len - i;
    char *p = s->data + i;
    if (n == 0 || p[n - 1] == '/') return 0;
    for (long k = 0; k < n; k++) {
        if (!p[k]) return 0;
        int start = k == 0 || p[k - 1] == '/';
        if (start && n - k >= 2 && p[k] == '.' && p[k + 1] == '.' && (n - k == 2 || p[k + 2] == '/')) return 0;
    }
    return i ? str_new(p, n) : s;
}

/* Normalizes the scope (directories for file effects) and adds it. 0 when the scope is invalid:
 * empty, absolute, or with a ".." component (E0506). */
int es_add_scope(EffSet *set, long bit, Str *str)
{
    long n = str->len;
    if (n == 0) return 0;
    if (bit == EF_SERVE && serve_address_parse(str, 0) < 0) return 0; /* spec 011: "PORT" or "0.0.0.0:PORT" */
    if (bit == EF_NET && net_scope_port(str) < 0) return 0; /* spec 039: "HOST" or "HOST:PORT" */
    GC_ROOT(&set);
    GC_ROOT(&str);
    if (bit == EF_PROCESS) { /* spec 017: kept verbatim after process_name */
        str = process_name(str);
        if (!str) {
            GC_UNROOT(2);
            return 0;
        }
    }
    if (bit & (EF_FREAD | EF_FWRITE | EF_FDELETE)) { /* env, net: kept verbatim */
        if (str->data[0] == '/') { /* absolute */
            GC_UNROOT(2);
            return 0;
        }
        SB *sb = sb_new();
        GC_ROOT(&sb);
        for (long i = 0; i < n;) {
            long j = i; /* the end of the component */
            while (j < n && str->data[j] != '/') j++;
            long len = j - i;
            char *c = str->data + i;
            if (len == 0 || (len == 1 && c[0] == '.')) {
                /* skipped */
            } else if (len == 2 && c[0] == '.' && c[1] == '.') {
                GC_UNROOT(3);
                return 0;
            } else {
                sb_bytes(sb, c, len);
                sb_byte(sb, '/');
            }
            i = j + 1;
        }
        str = sb_finish(sb);
        GC_UNROOT(1);
    }
    es_put_pair(set, bit, str);
    GC_UNROOT(2);
    return 1;
}

/* Puts every scope pair of src for bit into dst. */
static void es_copy_pairs(EffSet *dst, EffSet *src, long bit)
{
    Vec *v = src->scopes;
    if (!v) return;
    GC_ROOT(&dst);
    GC_ROOT(&v);
    for (long i = 0; i < v->n; i++) {
        EffScope *p = vec_get(v, i);
        if (p->bit == bit) es_put_pair(dst, bit, p->str);
    }
    GC_UNROOT(2);
}

/* 1 when a scope of set for bit covers str: a byte prefix for directories, equal otherwise (and a
 * `net` host covers its ports). */
int es_pair_covered(EffSet *set, long bit, Str *str)
{
    Vec *v = set->scopes;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++) {
        EffScope *p = vec_get(v, i);
        if (p->bit != bit) continue;
        if (bit & (EF_FREAD | EF_FWRITE | EF_FDELETE)) { /* prefix test */
            if ((unsigned long)str->len < (unsigned long)p->str->len) continue;
            if (mem_eq(p->str->data, str->data, p->str->len)) return 1;
        } else if (str_eq(p->str, str)) {
            return 1;
        } else if (bit == EF_NET && str->len > p->str->len && str->data[p->str->len] == ':' &&
                   mem_eq(p->str->data, str->data, p->str->len) && !net_scope_port(p->str)) {
            return 1; /* spec 039: a bare host covers that host with any port */
        }
    }
    return 0;
}

/* 1 when the set has exactly this scope pair. */
int es_has_pair(EffSet *set, long bit, Str *str)
{
    Vec *v = set->scopes;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++) {
        EffScope *p = vec_get(v, i);
        if (p->bit == bit && str_eq(p->str, str)) return 1;
    }
    return 0;
}

/* ---- effect parameters (ES_EVARS: TY_PARAM types with TPC_EFFECTS) ---- */
int es_has_evar(EffSet *set, void *param)
{
    if (!set || !set->evars) return 0;
    for (long i = 0; i < set->evars->n; i++)
        if (set->evars->data[i].p == param) return 1;
    return 0;
}

void es_add_evar(EffSet *set, void *param)
{
    if (es_has_evar(set, param)) return;
    GC_ROOT(&set);
    GC_ROOT(&param);
    if (!set->evars) set->evars = vec_new();
    vec_push(set->evars, param);
    GC_UNROOT(2);
}

/* Adds to dst the effect parameters of a that b (which may be 0) does not have. */
static void es_evars_diff(EffSet *dst, EffSet *a, EffSet *b)
{
    Vec *v = a->evars;
    if (!v) return;
    for (long i = 0; i < v->n; i++) {
        void *p = vec_get(v, i);
        if (!es_has_evar(b, p)) es_add_evar(dst, p);
    }
}

long es_evar_count(EffSet *set)
{
    return set->evars ? set->evars->n : 0;
}

EffSet *es_union(EffSet *a, EffSet *b)
{
    GC_ROOT(&a);
    GC_ROOT(&b);
    EffSet *u = es_new((long)(a->mask | b->mask));
    GC_ROOT(&u);
    for (long bit = 1; bit <= EF_ALL; bit <<= 1) {
        if (!(u->mask & (unsigned long)bit)) continue;
        if ((a->mask & (unsigned long)bit) && !es_scoped(a, bit)) continue; /* a has it unscoped */
        if ((b->mask & (unsigned long)bit) && !es_scoped(b, bit)) continue; /* b has it unscoped */
        es_copy_pairs(u, a, bit);
        es_copy_pairs(u, b, bit);
        if (bit != EF_NET) continue;
        Vec *all = u->scopes; /* spec 039: a host makes its ports redundant ("h", "h:1" is "h") */
        GC_ROOT(&all);
        u->scopes = 0;
        for (long i = 0; all && i < all->n; i++) {
            EffScope *p = vec_get(all, i);
            int covered = 0;
            for (long k = 0; p->bit == bit && k < all->n && !covered; k++) {
                EffScope *q = vec_get(all, k);
                covered = q->bit == bit && q->str->len < p->str->len && p->str->data[q->str->len] == ':' &&
                          mem_eq(q->str->data, p->str->data, q->str->len) && !net_scope_port(q->str);
            }
            if (!covered) es_put_pair(u, p->bit, p->str);
        }
        GC_UNROOT(1);
    }
    /* kept even where the mask has the bit: a builtin use makes every declared scope of it count as
     * needed (es_unused) */
    u->any = a->any | b->any;
    es_evars_diff(u, a, 0);
    es_evars_diff(u, b, 0);
    GC_UNROOT(3);
    return u;
}

/* The part of requirement r that declaration d does not cover. A bit in the result's mask with no
 * pairs means "needed unscoped"; pairs are the scopes d does not cover. */
EffSet *es_missing(EffSet *d, EffSet *r)
{
    GC_ROOT(&d);
    GC_ROOT(&r);
    EffSet *m = es_new(0);
    GC_ROOT(&m);
    for (long bit = 1; bit <= EF_ALL; bit <<= 1) {
        if (r->mask & (unsigned long)bit) {
            if ((d->any & (unsigned long)bit) && !(d->mask & (unsigned long)bit)) {
                /* spec 029: d from a type's effect argument (`Route[uses file.read]`): types carry no
                   scopes, so it allows the effect in any scope */
            } else if (!(d->mask & (unsigned long)bit)) { /* d lacks the bit: copy r's form of it */
                if (es_scoped(r, bit))
                    es_copy_pairs(m, r, bit);
                else
                    m->mask |= (unsigned long)bit;
            } else if (es_scoped(d, bit)) { /* (d unscoped: covered) */
                if (!es_scoped(r, bit)) {
                    m->mask |= (unsigned long)bit; /* d scoped, r unscoped: needs the whole effect */
                } else { /* both scoped: each r scope needs a covering d scope */
                    Vec *rs = r->scopes;
                    for (long i = 0; i < rs->n; i++) {
                        EffScope *p = vec_get(rs, i);
                        if (p->bit == bit && !es_pair_covered(d, bit, p->str)) es_put_pair(m, bit, p->str);
                    }
                }
            }
        }
        if ((r->any & (unsigned long)bit) && !((d->mask | d->any | m->mask) & (unsigned long)bit))
            m->any |= (unsigned long)bit; /* needed in any scope */
    }
    es_evars_diff(m, r, d);
    GC_UNROOT(3);
    return m;
}

int es_is_empty(EffSet *set)
{
    unsigned long v = set->mask | set->any;
    if (set->evars) v |= (unsigned long)set->evars->n;
    return v == 0;
}

int es_covers(EffSet *d, EffSet *r)
{
    return es_is_empty(es_missing(d, r));
}

/* 1 when some missing bit is absent from d altogether (E0501), 0 when only scopes fall short
 * (E0503). */
int es_lacks_bits(EffSet *d, EffSet *missing)
{
    unsigned long v = (missing->mask | missing->any) & ~(d->mask | d->any);
    if (missing->evars) v |= (unsigned long)missing->evars->n;
    return v != 0;
}

/* 1 when some scope pair of r is covered by the single-pair set one. */
static int es_any_pair_covered(EffSet *r, EffSet *one)
{
    Vec *v = r->scopes;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++) {
        EffScope *p = vec_get(v, i);
        if (es_pair_covered(one, p->bit, p->str)) return 1;
    }
    return 0;
}

/* The part of declaration d that requirement r never needs: whole effects, and scopes under which no
 * required scope lies. Scopes count as used when r needs the effect in any scope. */
EffSet *es_unused(EffSet *d, EffSet *r)
{
    GC_ROOT(&d);
    GC_ROOT(&r);
    EffSet *u = es_new(0);
    GC_ROOT(&u);
    for (long bit = 1; bit <= EF_ALL; bit <<= 1) {
        if (!(d->mask & (unsigned long)bit)) continue;
        if (!((r->mask | r->any) & (unsigned long)bit)) { /* never needed: the whole effect is unused */
            if (es_scoped(d, bit))
                es_copy_pairs(u, d, bit);
            else
                u->mask |= (unsigned long)bit;
            continue;
        }
        if (r->any & (unsigned long)bit) continue; /* needed in any scope: every scope counts as used */
        if (!(r->mask & (unsigned long)bit)) continue; /* only scoped strict requirements leave scopes */
        if (!es_scoped(r, bit) || !es_scoped(d, bit)) continue;
        Vec *ds = d->scopes;
        GC_ROOT(&ds);
        for (long i = 0; i < ds->n; i++) {
            EffScope *p = vec_get(ds, i);
            if (p->bit != bit) continue;
            GC_ROOT(&p);
            EffSet *one = es_new(0); /* does some required scope lie under this declared one? */
            GC_ROOT(&one);
            es_put_pair(one, bit, p->str);
            if (!es_any_pair_covered(r, one)) es_put_pair(u, bit, p->str);
            GC_UNROOT(2);
        }
        GC_UNROOT(1);
    }
    es_evars_diff(u, d, r);
    GC_UNROOT(3);
    return u;
}

/* a without b: bits b holds unscoped are dropped, b's scope pairs are removed, and a scoped bit left
 * with no pairs is dropped. */
EffSet *es_minus(EffSet *a, EffSet *b)
{
    GC_ROOT(&a);
    GC_ROOT(&b);
    EffSet *m = es_new(0);
    GC_ROOT(&m);
    for (long bit = 1; bit <= EF_ALL; bit <<= 1) {
        if (!(a->mask & (unsigned long)bit)) continue;
        if ((b->mask & (unsigned long)bit) && !es_scoped(b, bit)) continue; /* b drops the whole effect */
        if (!es_scoped(a, bit)) {
            m->mask |= (unsigned long)bit; /* a unscoped: keep the bit */
            continue;
        }
        Vec *as = a->scopes;
        for (long i = 0; i < as->n; i++) {
            EffScope *p = vec_get(as, i);
            if (p->bit == bit && !es_has_pair(b, bit, p->str)) es_put_pair(m, bit, p->str);
        }
    }
    m->any = a->any & ~(b->mask | b->any);
    es_evars_diff(m, a, b);
    GC_UNROOT(3);
    return m;
}

int es_equal(EffSet *a, EffSet *b)
{
    if (a->mask != b->mask || a->any != b->any) return 0;
    long n = es_evar_count(a);
    if (es_evar_count(b) != n) return 0;
    for (long i = 0; i < n; i++)
        if (!es_has_evar(b, vec_get(a->evars, i))) return 0;
    n = vec_len(a->scopes);
    if (vec_len(b->scopes) != n) return 0;
    for (long i = 0; i < n; i++) {
        EffScope *p = vec_get(a->scopes, i);
        EffScope *q = vec_get(b->scopes, i);
        if (p->bit != q->bit || !str_eq(p->str, q->str)) return 0;
    }
    return 1;
}

/* A scope as a vo string literal; the working directory ("") is "./". */
void es_render_scope(SB *sb, Str *str)
{
    sb_byte(sb, '"');
    if (str->len == 0) {
        sb_cstr(sb, "./");
    } else {
        for (long i = 0; i < str->len; i++) {
            char c = str->data[i];
            if (c == '"' || c == '\\') sb_byte(sb, '\\');
            sb_byte(sb, (unsigned char)c);
        }
    }
    sb_byte(sb, '"');
}

/* Canonical text: effects in bit order, scopes in parentheses; nothing for the empty set. */
void es_render(EffSet *set, SB *sb)
{
    int first = 1;
    for (long bit = 1; bit <= EF_ALL; bit <<= 1) {
        if (!((set->mask | set->any) & (unsigned long)bit)) continue;
        if (!first) sb_cstr(sb, ", ");
        first = 0;
        sb_cstr(sb, es_bit_name(bit));
        if (!(set->mask & (unsigned long)bit) || !es_scoped(set, bit)) continue;
        sb_byte(sb, '(');
        int first_scope = 1;
        for (long i = 0; i < set->scopes->n; i++) {
            EffScope *p = vec_get(set->scopes, i);
            if (p->bit != bit) continue;
            if (!first_scope) sb_cstr(sb, ", ");
            first_scope = 0;
            es_render_scope(sb, p->str);
        }
        sb_byte(sb, ')');
    }
    Vec *evars = set->evars; /* effect parameters by name */
    if (!evars) return;
    for (long i = 0; i < evars->n; i++) {
        if (!first) sb_cstr(sb, ", ");
        first = 0;
        sb_str(sb, ty_param_name(vec_get(evars, i)));
    }
}

static EffSet *es_subst_raw(EffSet *set, Node *owner, Vec *args)
{
    if (!es_evar_count(set)) return set;
    EffSet *r = es_new((long)set->mask); /* the same bits, scopes and any-scope bits, no parameters */
    r->scopes = set->scopes;
    r->any = set->any;
    for (long i = 0; i < set->evars->n; i++) {
        Type *p = vec_get(set->evars, i);
        if (owner == (Node *)-1L || p->decl.n == owner) { /* owner -1: any owner (a frame's own) */
            if ((unsigned long)p->a.i < (unsigned long)args->n) {
                Type *bound = args->data[p->a.i].t;
                if (bound && bound->kind == TY_EFFSET) { /* a binding holds the effect set */
                    r = es_union(r, bound->decl.es);
                    continue;
                }
            }
        }
        es_add_evar(r, p);
    }
    return r;
}

/* The set with owner's effect parameters replaced by the effect sets bound in args. Its results are
 * permanent: they are kept in frames and closures across collections and tests (spec 003). */
EffSet *es_subst(EffSet *set, Node *owner, Vec *args)
{
    perm_begin();
    EffSet *r = es_subst_raw(set, owner, args);
    perm_end();
    return r;
}
