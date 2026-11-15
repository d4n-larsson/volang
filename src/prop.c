/* Property tests (spec 005 US2, research R6-R10; ported from src/prop.s, spec 008): which types can
 * be generated, the generator, deep copies, type-directed shrinking and the property runner.
 * Generated values live in the collected heap: locals that hold them across an allocation are
 * rooted (GC_ROOT, research R4); pr_args/pr_best/pr_cand are globals.
 * ponytail: a fixed 100 cases / 1,000 attempts per property; an option if real properties need more.
 * ponytail: at most 2,000 shrink runs per property; raise it if counterexamples come out unshrunk.
 * ponytail: greedy shrinking (first candidate that still fails); locally minimal, not globally. */
#include "vo.h"

#define PR_CASES 100
#define PR_ATTEMPTS 1000
#define PR_BUDGET 2000
#define PR_DEPTH 6
enum { O_PASS, O_DISCARD, O_FAIL, O_ERROR, O_TIMEOUT, O_OOM }; /* pr_try outcomes */
enum { ST_PASS, ST_FAIL, ST_ERROR, ST_TIMEOUT };               /* test statuses (testrun.c) */

typedef union {
    long i;
    double d;
} Bits;

static unsigned long pg_state; /* the generator (SplitMix64), separate from random_int's */
long pr_args;                  /* List *: the arguments being generated or run */
long pr_best;                  /* List *: the smallest failing arguments so far */
long pr_cand;                  /* Vec *: the candidates of the parameter being shrunk */
long pr_cases;                 /* accepted cases run (including the failing one) */
long pr_discarded;
long pr_shrinks;
long pr_stopped; /* 1 when the time limit cut shrinking short */
long pr_starved; /* 1 when too many inputs were discarded */
static long pr_kind; /* the last failure: the expect node, or the runtime code */

char assume_code[] = "ASSUME"; /* the internal discard raised by assume(false); never printed */

static const char alphabet[] = "abcdeabcde0123456789 ,;:-_#";
#define ALPHABET_N 27
static char *const multibyte[] = {"\303\251", "\303\237", "\346\227\245", "\360\237\230\200"}; /* é ß 日 😀 */
static const long int_edges[] = {0, 1, -1, 0x7fffffffffffffffL, (long)0x8000000000000000UL, 0x7ffffffffffffffeL,
                                 (long)0x8000000000000001UL};
static const long float_edges[] = {0, (long)0x8000000000000000UL, 0x3ff0000000000000L, (long)0xbff0000000000000UL,
                                   0x3fe0000000000000L, 0x430c6bf526340000L, (long)0xc30c6bf526340000UL};

/* ---- the generator ---- */

/* The next 64 bits (SplitMix64). */
static unsigned long pr_next(void)
{
    unsigned long z = pg_state += 0x9e3779b97f4a7c15UL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9UL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebUL;
    return z ^ (z >> 31);
}

/* Uniform in 0..n-1 (n > 0). */
static long pr_below(long n)
{
    return (long)(pr_next() % (unsigned long)n);
}

/* Case case_no of the property at item index item: any case can be regenerated alone. */
void prop_seed(long seed, long item, long case_no)
{
    pg_state = (unsigned long)seed ^ ((unsigned long)item << 32) ^ (unsigned long)case_no;
}

/* fcvt.l.d with rtz, as RISC-V does it (research R13): NaN and too large give the largest value,
 * too small the smallest. */
static long trunc_rv(double d)
{
    if (d != d || d >= 9223372036854775808.0) return 0x7fffffffffffffffL;
    if (d < -9223372036854775808.0) return (long)0x8000000000000000UL;
    return (long)d;
}

/* ---- types ---- */

/* The number of fields of a record type or cases of a variant type. */
static long pg_nmembers(Type *t)
{
    long n = 0;
    while (member_at(t->decl.n->b.v, n)) n++;
    return n;
}

/* Field i's type (records) or case i's payload type, 0 when it has none (variants). */
static Type *pg_member(Type *t, long i)
{
    if (t->kind != TY_RECORD) return case_payload(t, i);
    return field_type_in(t, member_at(t->decl.n->b.v, i)->b.n->type);
}

static int pg_on_stack(Vec *stack, Type *t)
{
    for (long i = 0; i < stack->n; i++)
        if (stack->data[i].t == t) return 1;
    return 0;
}

/* target 0: 1 when type contains a function type or a type parameter. Otherwise 1 when type
 * contains target. Each record or variant is entered once along a path. */
static int pg_find(Type *t, Type *target, Vec *stack)
{
    if (!t) return 0;
    if (t == target) return 1;
    switch (t->kind) {
    case TY_FUNC:
    case TY_PARAM: return !target; /* functions matter only when looking for them */
    case TY_LIST:
    case TY_RESULT: return pg_find(t->a.t, target, stack);
    case TY_MAP: return pg_find(t->a.t, target, stack) || pg_find(t->b.t, target, stack);
    case TY_RECORD:
    case TY_VARIANT: {
        if (pg_on_stack(stack, t)) return 0;
        GC_ROOT(&stack); /* at run time a heap vec; pg_member may allocate (generic instances) */
        vec_push(stack, t);
        long n = pg_nmembers(t);
        int found = 0;
        for (long i = 0; i < n && !found; i++) {
            Type *m = pg_member(t, i);
            if (m) found = pg_find(m, target, stack);
        }
        stack->n--; /* pop */
        GC_UNROOT(1);
        return found;
    }
    default: return 0;
    }
}

/* 1 when a finite value of type exists (least fixpoint: a type already being entered counts as
 * having none along that path). */
static int pg_finite(Type *t, Vec *stack)
{
    if (t->kind != TY_RECORD && t->kind != TY_VARIANT) return 1;
    if (pg_on_stack(stack, t)) return 0;
    vec_push(stack, t);
    long n = pg_nmembers(t);
    int record = t->kind == TY_RECORD; /* a record: every field finite; a variant: one case */
    int result = record;              /* no member left: a record is finite, a variant is not */
    for (long i = 0; i < n; i++) {
        Type *m = pg_member(t, i);
        if (!m) { /* a case without a payload */
            result = 1;
            break;
        }
        int finite = pg_finite(m, stack);
        if (record && !finite) {
            result = 0;
            break;
        }
        if (!record && finite) {
            result = 1;
            break;
        }
    }
    stack->n--; /* pop */
    return result;
}

/* 0 when values of type can be generated; 1 when it contains a function type (or a type
 * parameter); 2 when it has no finite value (research R6). */
int prop_generatable(Type *t)
{
    if (!t || t->kind == TY_ERROR) return 0;
    Vec *stack = vec_new();
    if (pg_find(t, 0, stack)) return 1;
    return (1 - pg_finite(t, stack)) * 2; /* finite: 0, not: 2 */
}

/* ---- generation (research R7) ---- */

/* A string of length 0..min(size/4 + 1, 32): mostly a-e, digits and separators, 1 in 16 multi-byte. */
static Str *pg_str(long size)
{
    long max = (long)((unsigned long)size >> 2) + 1;
    if (max > 32) max = 32;
    long n = pr_below(max + 1);
    SB *sb = sb_new();
    while (n--) {
        if (!pr_below(16))
            sb_cstr(sb, multibyte[pr_below(4)]);
        else
            sb_byte(sb, alphabet[pr_below(ALPHABET_N)]);
    }
    return sb_finish(sb);
}

/* A collection length: uniform in 0..min(size/8 + 1, 16). */
static long pg_count(long size)
{
    long max = (long)((unsigned long)size >> 3) + 1;
    if (max > 16) max = 16;
    return pr_below(max + 1);
}

/* Variants at the depth limit: the pick-th case without a payload or with a payload that does not
 * contain the variant again; returns the case index, or -1 - (the number of such cases) when
 * pick is past them. */
static long pg_small_cases(Type *t, long pick)
{
    long n = pg_nmembers(t), seen = 0;
    for (long c = 0; c < n; c++) {
        Type *m = pg_member(t, c);
        if (m && pg_find(m, t, vec_new())) continue;
        if (seen == pick) return c;
        seen++;
    }
    return -1 - seen;
}

static Variant *pg_new_variant(long tag, long payload)
{
    GC_ROOT(&payload);
    Variant *v = alloc_obj(OBJ_VARIANT, VARIANT_SIZE);
    GC_UNROOT(1);
    v->tag = (long)(uint32_t)tag;
    v->pay.i = payload;
    return v;
}

/* A new value of type (research R7). depth bounds variant nesting. */
long prop_gen(Type *t, long size, long depth)
{
    if (size < 0) size = 0;
    switch (t->kind) {
    case TY_INT: /* int: 1 in 8 an edge, else -size..size */
        if (!pr_below(8)) return int_edges[pr_below(7)];
        return pr_below(2 * size + 1) - size;
    case TY_FLOAT: { /* float: 1 in 8 an edge, else (-size..size) / 2^(0..6) */
        if (!pr_below(8)) return float_edges[pr_below(7)];
        long v = pr_below(2 * size + 1) - size;
        long k = pr_below(7);
        Bits b = {.d = (double)v / (double)(1L << k)};
        return b.i;
    }
    case TY_BOOL: return pr_below(2);
    case TY_STR: return (long)pg_str(size);
    case TY_LIST: { /* list[T] */
        long n = pg_count(size);
        List *l = list_new(n);
        GC_ROOT(&l);
        while (n--) list_push(l, prop_gen(t->a.t, (long)((unsigned long)size >> 1), depth));
        GC_UNROOT(1);
        return (long)l;
    }
    case TY_MAP: { /* map[K, V]: duplicate keys collapse */
        long n = pg_count(size);
        Map *m = map_new();
        GC_ROOT(&m);
        while (n--) {
            long k = prop_gen(t->a.t, (long)((unsigned long)size >> 1), depth);
            GC_ROOT(&k);
            long v = prop_gen(t->b.t, (long)((unsigned long)size >> 1), depth);
            map_set(m, k, v, t->a.t);
            GC_UNROOT(1);
        }
        GC_UNROOT(1);
        return (long)m;
    }
    case TY_RESULT: { /* result[T]: 1 in 4 err */
        if (!pr_below(4)) return (long)make_result(1, (long)pg_str(size));
        long v = 0;
        if (t->a.t->kind != TY_VOID) v = prop_gen(t->a.t, size, depth);
        return (long)make_result(0, v);
    }
    case TY_RECORD: { /* record: each field with size - 1 */
        long n = pg_nmembers(t);
        Record *r = alloc_obj(OBJ_RECORD, n * 8);
        for (long i = 0; i < n; i++) r->fields[i].i = 0;
        GC_ROOT(&r);
        for (long i = 0; i < n; i++) {
            Type *ft = pg_member(t, i);
            long v = prop_gen(ft, size - 1, depth);
            r->fields[i].i = v;
        }
        GC_UNROOT(1);
        return (long)r;
    }
    case TY_VARIANT: { /* variant: past the depth budget only the small cases */
        long c;
        long small = depth > 0 ? 0 : -1 - pg_small_cases(t, -1); /* count them */
        if (small)
            c = pg_small_cases(t, pr_below(small));
        else
            c = pr_below(pg_nmembers(t)); /* none: any case */
        long pay = 0;
        Type *pt = pg_member(t, c);
        if (pt) pay = prop_gen(pt, (long)((unsigned long)size >> 1), depth - 1);
        return (long)pg_new_variant(c, pay);
    }
    default: return 0;
    }
}

/* A deep copy: lists, maps, records, variants and results are new; strings are shared. */
long prop_copy(long value, Type *t)
{
    switch (t->kind) {
    case TY_LIST: {
        List *src = (List *)value;
        GC_ROOT(&src);
        List *l = list_new(src->len);
        GC_ROOT(&l);
        for (long i = 0; i < src->len; i++) list_push(l, prop_copy(src->data[i].i, t->a.t));
        GC_UNROOT(2);
        return (long)l;
    }
    case TY_MAP: {
        Map *src = (Map *)value;
        GC_ROOT(&src);
        Map *m = map_new();
        GC_ROOT(&m);
        for (long i = 0; i < src->used; i++) {
            MapEntry *e = &src->entries[i];
            if (!e->live) continue;
            long v = prop_copy(e->val.i, t->b.t);
            map_set(m, src->entries[i].key.i, v, t->a.t);
        }
        GC_UNROOT(2);
        return (long)m;
    }
    case TY_RESULT: {
        Variant *src = (Variant *)value;
        long tag = (long)(uint32_t)src->tag;
        long pay = src->pay.i;
        if (!tag && t->a.t->kind != TY_VOID) pay = prop_copy(pay, t->a.t); /* err: the str is shared */
        return (long)make_result((int)tag, pay);
    }
    case TY_RECORD: {
        Record *src = (Record *)value;
        GC_ROOT(&src);
        long n = pg_nmembers(t);
        Record *r = alloc_obj(OBJ_RECORD, n * 8);
        GC_ROOT(&r);
        for (long i = 0; i < n; i++) {
            r->fields[i] = src->fields[i]; /* until the copy replaces it: the collector sees no garbage */
            Type *ft = pg_member(t, i);
            long v = prop_copy(src->fields[i].i, ft);
            r->fields[i].i = v;
        }
        GC_UNROOT(2);
        return (long)r;
    }
    case TY_VARIANT: {
        Variant *src = (Variant *)value;
        long tag = (long)(uint32_t)src->tag;
        long pay = src->pay.i;
        Type *pt = pg_member(t, tag);
        if (pt) pay = prop_copy(pay, pt);
        return (long)pg_new_variant(tag, pay);
    }
    default: return value;
    }
}

/* ---- shrinking (research R8) ---- */

/* A copy of list with element i replaced by value. */
static List *pg_list_with(List *list, long i, long value)
{
    GC_ROOT(&list);
    GC_ROOT(&value);
    List *l = list_new(list->len);
    GC_ROOT(&l);
    for (long k = 0; k < list->len; k++) list_push(l, k == i ? value : list->data[k].i);
    GC_UNROOT(3);
    return l;
}

/* A copy of list without element i. */
static List *pg_list_without(List *list, long i)
{
    GC_ROOT(&list);
    List *l = list_new(list->len);
    GC_ROOT(&l);
    for (long k = 0; k < list->len; k++)
        if (k != i) list_push(l, list->data[k].i);
    GC_UNROOT(2);
    return l;
}

/* A copy of map where entry index `entry` gets value `value`, or, when entry is
 * (1 << 62 | index), where that entry is dropped. */
static Map *pg_map_edit(Map *map, long entry, long value, Type *map_type)
{
    GC_ROOT(&map);
    GC_ROOT(&value);
    Map *m = map_new();
    GC_ROOT(&m);
    for (long i = 0; i < map->used; i++) {
        MapEntry *e = &map->entries[i];
        if (!e->live || ((1L << 62) | i) == entry) continue; /* dropped */
        map_set(m, e->key.i, i == entry ? value : e->val.i, map_type->a.t); /* (replaced) */
    }
    GC_UNROOT(3);
    return m;
}

/* Candidates for a list (or, with chars, a string as its list of characters): empty, the two
 * halves, each element removed (from the end), then each element shrunk in place (chars: replaced
 * by "a"). */
static Vec *pg_seq(List *list, Type *elem, int chars)
{
    GC_ROOT(&list);
    Vec *out = vec_new();
    GC_ROOT(&out);
    long n = list->len;
    if (n) {
        vec_push(out, list_new(0));
        if (n >= 2) {
            vec_push(out, list_slice(list, 0, n >> 1, 0)); /* halves */
            vec_push(out, list_slice(list, n >> 1, n, 0));
            for (long i = n - 1; i >= 0; i--) vec_push(out, pg_list_without(list, i)); /* each removed */
        }
        Vec *cands = 0;
        GC_ROOT(&cands);
        for (long i = 0; i < n; i++) { /* each element shrunk in place */
            long x = list->data[i].i;
            if (chars) { /* a character: "a" unless it is "a" */
                Str *s = (Str *)x;
                if (s->len == 1 && s->data[0] == 'a') continue;
                vec_push(out, pg_list_with(list, i, (long)str_from_cstr("a")));
                continue;
            }
            cands = prop_shrinks(x, elem);
            for (long j = 0; j < cands->n; j++) vec_push(out, pg_list_with(list, i, cands->data[j].i));
        }
        GC_UNROOT(1);
    }
    GC_UNROOT(2);
    return out;
}

/* The shrink candidates of value in research R8's order: a vec of new values (value itself is
 * never changed). */
Vec *prop_shrinks(long value, Type *t)
{
    GC_ROOT(&value);
    Vec *out = vec_new();
    GC_ROOT(&out);
    Vec *cands = 0; /* the candidates of a part */
    GC_ROOT(&cands);
    switch (t->kind) {
    case TY_INT: { /* int: 0, x/2, x - sign(x) */
        if (!value) break;
        vec_push(out, 0);
        long half = value / 2;
        if (half) vec_push(out, (void *)half);
        long step = value > 0 ? value - 1 : value + 1;
        if (step && step != half) vec_push(out, (void *)step);
        break;
    }
    case TY_FLOAT: { /* float: 0.0, the truncated value, x/2 */
        Bits x = {.i = value};
        if (x.d == 0.0) break;
        vec_push(out, 0);
        long trunc = trunc_rv(x.d);
        Bits t2 = {.d = (double)trunc};
        long whole = 0;
        if (!(x.d == t2.d) && trunc) {
            whole = t2.i;
            vec_push(out, (void *)whole);
        }
        Bits half = {.d = x.d * 0.5};
        if (half.d == 0.0 || half.i == whole) break;
        vec_push(out, (void *)half.i);
        break;
    }
    case TY_BOOL:
        if (value) vec_push(out, 0);
        break;
    case TY_STR: { /* str: as its list of characters, joined back */
        cands = pg_seq(rt_chars((Str *)value), 0, 1);
        Str *empty = str_from_cstr("");
        GC_ROOT(&empty);
        for (long i = 0; i < cands->n; i++) vec_push(out, rt_join(cands->data[i].p, empty));
        GC_UNROOT(1);
        break;
    }
    case TY_LIST: out = pg_seq((List *)value, t->a.t, 0); break;
    case TY_MAP: { /* map: empty, each entry removed, each value shrunk */
        Map *m = (Map *)value;
        if (!m->count) break;
        vec_push(out, map_new());
        for (long e = 0; e < m->used; e++)
            if (m->entries[e].live) vec_push(out, pg_map_edit(m, (1L << 62) | e, 0, t));
        for (long e = 0; e < m->used; e++) {
            if (!m->entries[e].live) continue;
            cands = prop_shrinks(m->entries[e].val.i, t->b.t);
            for (long j = 0; j < cands->n; j++) vec_push(out, pg_map_edit(m, e, cands->data[j].i, t));
        }
        break;
    }
    case TY_RESULT: { /* result: the payload shrunk, in the same case */
        Variant *v = (Variant *)value;
        long tag = (long)(uint32_t)v->tag;
        Type *pt = t_str; /* err carries a str */
        if (!tag) {
            pt = t->a.t;
            if (pt->kind == TY_VOID) break;
        }
        cands = prop_shrinks(v->pay.i, pt);
        for (long j = 0; j < cands->n; j++) vec_push(out, make_result((int)tag, cands->data[j].i));
        break;
    }
    case TY_RECORD: { /* record: each field shrunk in place */
        Record *r = (Record *)value;
        long n = pg_nmembers(t);
        for (long i = 0; i < n; i++) {
            Type *ft = pg_member(t, i);
            cands = prop_shrinks(r->fields[i].i, ft);
            for (long j = 0; j < cands->n; j++) {
                Record *copy = alloc_obj(OBJ_RECORD, n * 8); /* a copy of the record ... */
                for (long k = 0; k < n; k++) copy->fields[k] = r->fields[k];
                copy->fields[i].i = cands->data[j].i; /* ... with field i replaced */
                vec_push(out, copy);
            }
        }
        break;
    }
    case TY_VARIANT: { /* variant: earlier cases without a payload, subtrees, the payload */
        Variant *v = (Variant *)value;
        long tag = (long)(uint32_t)v->tag;
        for (long c = 0; c < tag; c++)
            if (!pg_member(t, c)) vec_push(out, pg_new_variant(c, 0));
        if (!v->pay.i) break;
        Type *pt = pg_member(t, tag); /* payload type */
        if (pt == t) { /* the payload is itself a value of this variant */
            vec_push(out, v->pay.p);
        } else if (pt->kind == TY_RECORD) { /* fields of the payload record that are values of this variant */
            long n = pg_nmembers(pt);
            for (long f = 0; f < n; f++)
                if (pg_member(pt, f) == t) vec_push(out, ((Record *)v->pay.p)->fields[f].p);
        }
        cands = prop_shrinks(v->pay.i, pt);
        for (long j = 0; j < cands->n; j++) vec_push(out, pg_new_variant(tag, cands->data[j].i));
        break;
    }
    default: break;
    }
    GC_UNROOT(3);
    return out;
}

/* ---- running a property (research R9, R10) ---- */

static int pr_streq(char *a, char *b)
{
    for (; *a == *b; a++, b++)
        if (!*a) return 1;
    return 0;
}

/* One run of the property body on a deep copy of args, under the test runner's recovery point.
 * The outcome (O_*); a failure's expect node or runtime code goes to pr_kind. */
static int pr_try(Node *test, List *args)
{
    long jb[JB_SIZE / 8]; /* the recovery buffer */
    Vec *params = test->d.fx->params;
    GC_ROOT(&args);
    List *copy = list_new(args->len);
    GC_ROOT(&copy);
    for (long i = 0; i < args->len; i++) {
        Type *pt = ((Node *)vec_get(params, i))->type;
        list_push(copy, prop_copy(args->data[i].i, pt));
    }
    tf_node = 0;
    rt_err_code = 0;
    recover_point = (long)jb;
    long status = jmp_save(jb); /* 0 ran through, 1 expect failed, 2 runtime error */
    if (!status) {
        rt_effects_start(1); /* every case starts from seed and clock defaults */
        tr_test(test);       /* spec 006: the trace's test record, for each case */
        vm_run(vm_push(test->d.fx->code, test, 0, &copy->data->i, 0));
    }
    GC_UNROOT(2);
    recover_point = 0;
    interp_reset();
    if (!status) return O_PASS;
    if (status == 1) {
        pr_kind = tf_node;
        return O_FAIL;
    }
    char *code = (char *)rt_err_code;
    if (code == assume_code) return O_DISCARD;
    if (pr_streq(code, "R0009")) return O_TIMEOUT;
    if (pr_streq(code, "R0006")) return O_OOM;
    pr_kind = (long)code;
    return O_ERROR;
}

/* Greedy shrinking of pr_best, parameter by parameter: the first candidate that fails the same way
 * (same outcome; same expect node, or same runtime code) replaces it, and the search restarts.
 * Stops at the budget or when the time limit is reached (pr_stopped). */
static void pr_shrink(Node *test, int outcome, long kind)
{
    Vec *params = test->d.fx->params;
    long runs = 0;
    for (long p = 0; p < params->n; p++) {
    restart:;
        Type *pt = ((Node *)vec_get(params, p))->type;
        pr_cand = (long)prop_shrinks(((List *)pr_best)->data[p].i, pt);
        for (long c = 0; c < ((Vec *)pr_cand)->n;) {
            if (runs >= PR_BUDGET) return;
            long cand = ((Vec *)pr_cand)->data[c].i;
            pr_args = (long)pg_list_with((List *)pr_best, p, cand);
            int o = pr_try(test, (List *)pr_args);
            runs++;
            c++;
            if (o == O_TIMEOUT) { /* the property's time limit: keep what we have */
                pr_stopped = 1;
                return;
            }
            if (o != outcome) continue;
            if (pr_kind != kind && !(outcome == O_ERROR && pr_streq((char *)pr_kind, (char *)kind))) continue;
            pr_best = pr_args; /* still fails the same way: the new best */
            pr_shrinks++;
            goto restart;
        }
    }
}

/* Runs property `test` (item index `item` in its file) until 100 cases pass or 1,000 attempts are
 * made, shrinking the first failure. Returns the test status (ST_*); the report reads pr_*.
 * The caller has started the heap and set the deadline for the whole property. */
int prop_run(Node *test, long item)
{
    Vec *params = test->d.fx->params;
    long np = params->n;
    pr_cases = 0;
    pr_discarded = 0;
    pr_shrinks = 0;
    pr_stopped = 0;
    pr_starved = 0;
    pr_best = 0;
    for (long attempts = 0; pr_cases < PR_CASES;) {
        if (attempts >= PR_ATTEMPTS) { /* too many discards: a failure at the `prop` keyword */
            pr_starved = 1;
            SB *sb = sb_new();
            sb_cstr(sb, "too many inputs were discarded by `assume` (");
            int_to_dec(sb, pr_cases);
            sb_cstr(sb, " of 1000 accepted)");
            tf_msg = (long)sb_finish(sb);
            tf_node = (long)test;
            tf_expected = 0;
            tf_actual = 0;
            return ST_FAIL;
        }
        prop_seed(opt_seed_set ? opt_seed : 1, item, attempts); /* --seed (default 1), item, attempt */
        pr_args = (long)list_new(np);
        for (long i = 0; i < np; i++) {
            Type *pt = ((Node *)vec_get(params, i))->type;
            long v = prop_gen(pt, attempts % 100 + 1, PR_DEPTH); /* size 1..100: early cases are tiny */
            list_push((List *)pr_args, v);
        }
        int o = pr_try(test, (List *)pr_args);
        attempts++;
        if (o == O_DISCARD) {
            pr_discarded++;
            continue;
        }
        pr_cases++;
        if (o == O_PASS) continue;
        pr_best = pr_args; /* the counterexample starts as this input */
        if (o == O_TIMEOUT) return ST_TIMEOUT; /* a timeout or out of memory: reported unshrunk */
        if (o == O_OOM) return ST_ERROR;
        pr_shrink(test, o, pr_kind);
        deadline_ms = 0; /* the report describes the final counterexample: run it again */
        pr_try(test, (List *)pr_best);
        return o == O_FAIL ? ST_FAIL : ST_ERROR;
    }
    return ST_PASS;
}

/* ---- reports (contracts/reports.md) ---- */

/* Parameter i's value in pr_best, rendered in vo syntax (a str). */
static Str *pr_value_text(long i, Node *test)
{
    Type *t = ((Node *)vec_get(test->d.fx->params, i))->type;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    render_value(sb, ((List *)pr_best)->data[i].i, t);
    GC_UNROOT(1);
    return sb_finish(sb);
}

static Str *pr_param_name(long i, Node *test)
{
    return ((Node *)vec_get(test->d.fx->params, i))->a.n->a.s;
}

/* The property members of a `test` record, after "ms". */
void prop_json(SB *sb, Node *test, int status)
{
    sb_cstr(sb, ",\"prop\":true,\"cases\":");
    int_to_dec(sb, pr_cases);
    if (pr_discarded) {
        sb_cstr(sb, ",\"discarded\":");
        int_to_dec(sb, pr_discarded);
    }
    if (!status || pr_starved) return;
    sb_cstr(sb, ",\"shrinks\":");
    int_to_dec(sb, pr_shrinks);
    sb_cstr(sb, ",\"counterexample\":[");
    for (long i = 0; i < test->d.fx->params->n; i++) {
        sb_cstr(sb, i ? ",{\"name\":" : "{\"name\":");
        json_str_into(sb, pr_param_name(i, test));
        sb_cstr(sb, ",\"value\":");
        json_str_into(sb, pr_value_text(i, test));
        sb_byte(sb, '}');
    }
    sb_byte(sb, ']');
    if (pr_stopped) sb_cstr(sb, ",\"shrink_stopped\":true");
}

/* "N cases, " inside a property's text line. */
void prop_cases_text(SB *sb)
{
    int_to_dec(sb, pr_cases);
    sb_cstr(sb, pr_cases == 1 ? " case, " : " cases, ");
}

/* After a failing property's details: one counterexample line per parameter, then the shrink line. */
void prop_text(SB *sb, Node *test, int status)
{
    if (!status || pr_starved) return;
    for (long i = 0; i < test->d.fx->params->n; i++) {
        sb_cstr(sb, "    counterexample: ");
        sb_str(sb, pr_param_name(i, test));
        sb_cstr(sb, " = ");
        sb_str(sb, pr_value_text(i, test));
        sb_byte(sb, '\n');
    }
    if (status == ST_TIMEOUT) return;
    sb_cstr(sb, pr_stopped ? "    shrinking stopped at the time limit after " : "    shrunk in ");
    int_to_dec(sb, pr_shrinks);
    sb_cstr(sb, pr_shrinks == 1 ? " step\n" : " steps\n");
}
