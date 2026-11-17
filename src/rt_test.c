/* expect / expect_eq: on failure, record it and return to the test runner's recovery point
 * (ported from src/rt_test.s, spec 008). */
#include "vo.h"

long tf_node;     /* the failing expect call */
long tf_msg;      /* Str * */
long tf_expected; /* rendered values (Str *) or 0 */
long tf_actual;

long tf_nops;        /* spec 018: operands shown for a failing comparison (0-2) */
long tf_op_text[2];  /* their source texts (Str *) */
long tf_op_val[2];   /* their values in vo syntax, shortened (Str *) */

/* spec 018: a literal, whose value is its text: a number, string or bool constant (negative numbers
 * too), or a list, record or variant case built only of literals (W0603, and the operands that
 * `expect` leaves out). */
int node_is_literal(Node *n)
{
    if (n->kind == N_UNARY && (n->a.n->kind == N_INT || n->a.n->kind == N_FLOAT)) n = n->a.n; /* -1, -2.5 */
    if (n->kind == N_INT || n->kind == N_FLOAT || n->kind == N_STR || n->kind == N_BOOL) return 1;
    if (n->kind == N_LIST) {
        for (long i = 0; i < n->a.v->n; i++)
            if (!node_is_literal(vec_get(n->a.v, i))) return 0;
        return 1;
    }
    if (n->kind == N_NEW_RECORD) {
        for (long i = 0; i < n->b.v->n; i++)
            if (!node_is_literal(((Node *)vec_get(n->b.v, i))->b.n)) return 0;
        return 1;
    }
    if (n->kind == N_VCASE) return !n->c.n || node_is_literal(n->c.n);
    return 0;
}

void rt_expect(long cond, Node *node)
{
    if (cond) return;
    tf_nops = 0;
    Node *c = node->b.v->data[0].n;
    if (c->kind == N_BINARY && c->d.i == OP_OR) { /* spec 018: both parts were false */
        Str *a = src_text(c->a.n);
        GC_ROOT(&a);
        Str *b = src_text(c->b.n);
        GC_ROOT(&b);
        tf_msg = (long)msg("`expect` failed: neither `$1` nor `$2` is true.", a, b, 0);
        GC_UNROOT(2);
        tf_node = (long)node;
        tf_expected = 0;
        tf_actual = 0;
        rt_recover(1);
    }
    tf_msg = (long)str_from_cstr("`expect` failed: the condition is false.");
    tf_node = (long)node;
    tf_expected = 0;
    tf_actual = 0;
    rt_recover(1);
}

/* spec 018: a failing comparison in `expect`, with the values of its operands that are not literals. */
void rt_expect_cmp(long cond, long left, long right, Node *call)
{
    if (cond) return;
    GC_ROOT(&left);
    GC_ROOT(&right);
    Node *c = call->b.v->data[0].n;
    Node *cmp = (call->flags & NF_EXPECT_NEG) ? c->a.n : c;
    Str *text = src_text(cmp);
    GC_ROOT(&text);
    tf_msg = (long)msg((call->flags & NF_EXPECT_NEG) ? "`expect` failed: `$1` is true." : "`expect` failed: `$1` is false.",
                       text, 0, 0);
    GC_UNROOT(1);
    tf_nops = 0;
    Node *sides[2] = {cmp->a.n, cmp->b.n};
    long vals[2] = {left, right};
    for (int i = 0; i < 2; i++) {
        if (node_is_literal(sides[i])) continue;
        long k = tf_nops++;
        tf_op_text[k] = (long)src_text(sides[i]);
        SB *sb = sb_new();
        GC_ROOT(&sb);
        render_capped(sb, vals[i], sides[i]->type);
        tf_op_val[k] = (long)sb_finish(sb);
        GC_UNROOT(1);
    }
    GC_UNROOT(2);
    tf_node = (long)call;
    tf_expected = 0;
    tf_actual = 0;
    rt_recover(1);
}

/* ---- spec 018 (research R6): the path to the first difference of expect_eq ---- */

long tf_diff_path;  /* Str *, or 0: no difference reported */
long tf_diff_exp;   /* rendered value at the path (Str *), or 0 when the expected value has none */
long tf_diff_act;   /* ... the actual value */
long tf_diff_char;  /* strings: the 1-based character of the first difference, else 0 */
long tf_diff_len_a; /* lengths when one side has no element at the path (else -1) */
long tf_diff_len_e;

static long dd_act, dd_exp; /* the values at the first difference */
static Type *dd_type;
static int dd_miss;         /* 1: the actual value has no element there, 2: the expected one */

static int first_diff(SB *path, long a, long e, Type *t);

/* Lists compared by position: the first index that differs, else the first one only one side has. */
static void diff_lists(SB *path, List *a, List *e, Type *elem)
{
    GC_ROOT(&a);
    GC_ROOT(&e);
    GC_ROOT(&elem);
    long n = a->len < e->len ? a->len : e->len;
    for (long i = 0; i < n; i++) {
        long len = path->len;
        sb_byte(path, '[');
        int_to_dec(path, i);
        sb_byte(path, ']');
        if (first_diff(path, a->data[i].i, e->data[i].i, elem)) {
            GC_UNROOT(3);
            return;
        }
        path->len = len;
    }
    sb_byte(path, '[');
    int_to_dec(path, n);
    sb_byte(path, ']');
    dd_type = elem;
    dd_miss = a->len < e->len ? 1 : 2;
    dd_act = dd_miss == 1 ? 0 : a->data[n].i;
    dd_exp = dd_miss == 2 ? 0 : e->data[n].i;
    tf_diff_len_a = a->len;
    tf_diff_len_e = e->len;
    GC_UNROOT(3);
}

/* Keyed contents: the first key of e (in its order) that a lacks or maps to a different value, else
 * the first key only a has. keys/vals are the contents in iteration order. */
static void diff_keyed(SB *path, List *ak, List *av, List *ek, List *ev, Type *kt, Type *vt)
{
    GC_ROOT(&ak);
    GC_ROOT(&av);
    GC_ROOT(&ek);
    GC_ROOT(&ev);
    for (int side = 0; side < 2; side++) {
        List *xk = side ? ak : ek, *xv = side ? av : ev, *yk = side ? ek : ak, *yv = side ? ev : av;
        for (long i = 0; i < xk->len; i++) {
            long j = 0;
            while (j < yk->len && !values_equal(yk->data[j].i, xk->data[i].i, kt)) j++;
            if (j < yk->len && (side || values_equal(xv->data[i].i, yv->data[j].i, vt))) continue;
            sb_byte(path, '[');
            render_capped(path, xk->data[i].i, kt);
            sb_byte(path, ']');
            if (j < yk->len) { /* the same key, different values */
                first_diff(path, yv->data[j].i, xv->data[i].i, vt);
            } else {
                dd_type = vt;
                dd_miss = side ? 2 : 1;
                dd_act = side ? xv->data[i].i : 0;
                dd_exp = side ? 0 : xv->data[i].i;
            }
            GC_UNROOT(4);
            return;
        }
    }
    GC_UNROOT(4);
}

/* The keys (or values) of a map in insertion order, as a list. */
static List *map_part(Map *m, int values)
{
    GC_ROOT(&m);
    List *out = list_new(m->used ? m->used : 1);
    GC_ROOT(&out);
    for (long i = 0; i < m->used; i++)
        if (m->entries[i].live) list_push(out, values ? m->entries[i].val.i : m->entries[i].key.i);
    GC_UNROOT(2);
    return out;
}

/* 1 when a and e differ; the path to the first difference is appended to path and dd_* describe it.
 * Leaves compare with values_equal, so the walk agrees with expect_eq's own result. */
static int first_diff(SB *path, long a, long e, Type *t)
{
    if (values_equal(a, e, t)) return 0;
    GC_ROOT(&a);
    GC_ROOT(&e);
    GC_ROOT(&t);
    t = rt_concrete(t);
    dd_type = t; /* a leaf, unless a case below goes deeper */
    dd_act = a;
    dd_exp = e;
    dd_miss = 0;
    switch (t->kind) {
    case TY_LIST: diff_lists(path, (List *)a, (List *)e, t->a.t); break;
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: {
        List *xa = coll_snapshot((Coll *)a);
        GC_ROOT(&xa);
        List *xe = coll_snapshot((Coll *)e);
        GC_ROOT(&xe);
        diff_lists(path, xa, xe, t->a.t);
        GC_UNROOT(2);
        break;
    }
    case TY_MAP:
    case TY_OMAP: {
        int om = t->kind == TY_OMAP;
        List *ak = om ? coll_snapshot((Coll *)a) : map_part((Map *)a, 0);
        GC_ROOT(&ak);
        List *av = om ? coll_values((Coll *)a) : map_part((Map *)a, 1);
        GC_ROOT(&av);
        List *ek = om ? coll_snapshot((Coll *)e) : map_part((Map *)e, 0);
        GC_ROOT(&ek);
        List *ev = om ? coll_values((Coll *)e) : map_part((Map *)e, 1);
        GC_ROOT(&ev);
        diff_keyed(path, ak, av, ek, ev, t->a.t, t->b.t);
        GC_UNROOT(4);
        break;
    }
    case TY_RECORD: {
        Record *ra = (Record *)a, *re = (Record *)e;
        for (long i = 0;; i++) {
            Node *m = member_at(t->decl.n->b.v, i);
            if (!m) break;
            Type *ft = field_type_in(t, m->b.n->type);
            if (values_equal(ra->fields[i].i, re->fields[i].i, ft)) continue;
            sb_byte(path, '.');
            sb_str(path, m->a.n->a.s);
            first_diff(path, ra->fields[i].i, re->fields[i].i, ft);
            break;
        }
        break;
    }
    case TY_VARIANT:
    case TY_RESULT: {
        Variant *va = (Variant *)a, *ve = (Variant *)e;
        long tag = (long)(uint32_t)ve->tag;
        if ((long)(uint32_t)va->tag != tag) break; /* different cases: the values themselves */
        Type *pt;
        if (t->kind == TY_RESULT) {
            pt = case_payload(t, tag);
            sb_cstr(path, tag ? ".err" : ".ok");
        } else {
            Node *c = member_at(t->decl.n->b.v, tag);
            pt = field_type_in(t, c->b.n->type);
            sb_byte(path, '.');
            sb_str(path, c->a.n->a.s);
        }
        first_diff(path, va->pay.i, ve->pay.i, pt);
        break;
    }
    default: break;
    }
    GC_UNROOT(3);
    return 1;
}

/* Up to 40 characters of s from character `from` (0-based) as a string literal. */
static Str *str_excerpt(Str *s, long from)
{
    long i = 0, c = 0;
    while (i < s->len && c < from) { /* to the character */
        i++;
        while (i < s->len && ((unsigned char)s->data[i] & 0xc0) == 0x80) i++;
        c++;
    }
    long j = i;
    for (long k = 0; j < s->len && k < 40; k++) {
        j++;
        while (j < s->len && ((unsigned char)s->data[j] & 0xc0) == 0x80) j++;
    }
    Str *part = str_new(s->data + i, j - i);
    GC_ROOT(&part);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    render_str_lit(sb, part);
    if (j < s->len) sb_cstr(sb, "...");
    GC_UNROOT(2);
    return sb_finish(sb);
}

/* Fills tf_diff_* for a failing expect_eq on a non-scalar, or on strings longer than 40 characters. */
static void expect_eq_diff(long actual, long expected, Type *type)
{
    tf_diff_path = 0;
    tf_diff_char = 0;
    tf_diff_len_a = tf_diff_len_e = -1;
    type = rt_concrete(type);
    if (type->kind == TY_STR) {
        Str *a = (Str *)actual, *e = (Str *)expected;
        long chars = 0, i = 0, first = -1, ca = 0;
        for (long k = 0; k < e->len; k++) chars += ((unsigned char)e->data[k] & 0xc0) != 0x80;
        for (long k = 0; k < a->len; k++) ca += ((unsigned char)a->data[k] & 0xc0) != 0x80;
        if (chars <= 40 && ca <= 40) return;
        long c = 0;
        for (i = 0; i < a->len && i < e->len; i++) { /* the first differing byte, as a character */
            if (a->data[i] != e->data[i]) {
                first = c;
                break;
            }
            if (((unsigned char)a->data[i] & 0xc0) != 0x80) c++;
        }
        if (first < 0) first = c;
        tf_diff_char = first + 1;
        tf_diff_path = (long)str_from_cstr("");
        tf_diff_exp = (long)str_excerpt(e, first);
        tf_diff_act = (long)str_excerpt(a, first);
        return;
    }
    if (type->kind == TY_INT || type->kind == TY_FLOAT || type->kind == TY_BOOL) return;
    SB *path = sb_new();
    GC_ROOT(&path);
    first_diff(path, actual, expected, type);
    if (!path->len) { /* the values differ at the top: the whole values say it already */
        GC_UNROOT(1);
        return;
    }
    tf_diff_path = (long)sb_finish(path);
    GC_UNROOT(1);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    tf_diff_exp = 0;
    if (dd_miss != 2) {
        render_capped(sb, dd_exp, dd_type);
        tf_diff_exp = (long)sb_finish(sb);
        sb = sb_new();
    }
    tf_diff_act = 0;
    if (dd_miss != 1) {
        render_capped(sb, dd_act, dd_type);
        tf_diff_act = (long)sb_finish(sb);
    }
    GC_UNROOT(1);
}

void rt_expect_eq(long actual, long expected, Node *node)
{
    Type *type = ((Node *)node->b.v->data[0].p)->type; /* the first argument's type */
    if (values_equal(actual, expected, type)) return;
    GC_ROOT(&actual);
    GC_ROOT(&expected);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    render_value(sb, actual, type);
    tf_actual = (long)sb_finish(sb);
    sb = sb_new();
    render_value(sb, expected, type);
    tf_expected = (long)sb_finish(sb);
    expect_eq_diff(actual, expected, type); /* spec 018 */
    GC_UNROOT(3);
    tf_msg = (long)str_from_cstr("`expect_eq` failed: the values differ.");
    tf_node = (long)node;
    rt_recover(1);
}
