/* Checker, spec 029: annotations on record fields (research R3, contracts/json.md). `@size(min, max)` on
 * `str` (characters), lists and maps; `@range(min, max)` on `int` and `float` (literals of the field's
 * type); `@pattern("re")` on `str` (checked as a regex literal); `@default(v)` on scalars, enum-like
 * variants and lists (`[]` only), satisfying the field's other limits; `@header("Name")` on scalars.
 * Every misuse is E0329. The annotations themselves are read at run time by src/json_derive.c. */
#include "vo.h"

static char *const annot_names[] = {"size", "range", "pattern", "default", "header", 0};

static void bad(Node *at, char *tmpl, Str *a)
{
    ck_err("E0329", at, msg(tmpl, a, 0, 0));
}

static double as_double(long bits)
{
    union {
        long i;
        double d;
    } u = {bits};
    return u.d;
}

/* An int literal, possibly negated: 1 and *v, or 0. */
static int lit_int(Node *e, long *v)
{
    int neg = 0;
    if (e->kind == N_UNARY && e->c.i == T_MINUS) {
        neg = 1;
        e = e->a.n;
    }
    if (!e || e->kind != N_INT) return 0;
    *v = neg ? -e->a.i : e->a.i;
    return 1;
}

static int lit_float(Node *e, double *v)
{
    int neg = 0;
    if (e->kind == N_UNARY && e->c.i == T_MINUS) {
        neg = 1;
        e = e->a.n;
    }
    if (!e || e->kind != N_FLOAT) return 0;
    *v = neg ? -as_double(e->a.i) : as_double(e->a.i);
    return 1;
}

/* Characters in a str (UTF-8 code points). */
long utf8_chars(Str *s)
{
    long n = 0;
    for (long i = 0; i < s->len; i++) n += ((unsigned char)s->data[i] & 0xC0) != 0x80;
    return n;
}

static Node *find_annot(Vec *v, char *name)
{
    for (long i = 0; v && i < v->n; i++) {
        Node *a = vec_get(v, i);
        if (a->a.n && str_eq_cstr(a->a.n->a.s, name)) return a;
    }
    return 0;
}

static int is_scalar(Type *t)
{
    return t->kind == TY_INT || t->kind == TY_FLOAT || t->kind == TY_BOOL || t->kind == TY_STR;
}

/* Whether t is a variant whose cases carry no values. */
int ty_enum_like(Type *t)
{
    if (!t || t->kind != TY_VARIANT) return 0;
    Vec *cases = t->decl.n->b.v;
    for (long i = 0; i < cases->n; i++) {
        Node *c = vec_get(cases, i);
        if (c->kind == N_CASE_DECL && c->b.n) return 0;
    }
    return 1;
}

/* The two bound arguments of @size / @range, checked; 0 on a reported problem. */
static int bounds(Node *a, Type *ft, int is_size, double *lo, double *hi)
{
    if (a->b.v->n != 2) {
        bad(a, is_size ? "`@size` takes two whole numbers: `@size(min, max)`."
                       : "`@range` takes two numbers: `@range(min, max)`.", 0);
        return 0;
    }
    for (int k = 0; k < 2; k++) {
        Node *e = vec_get(a->b.v, k);
        long iv;
        double fv;
        if (is_size || ft->kind == TY_INT) {
            if (!lit_int(e, &iv) || (is_size && iv < 0)) {
                bad(e, is_size ? "`@size` bounds are whole numbers from 0." : "This field is an `int`: its range is whole numbers.", 0);
                return 0;
            }
            fv = (double)iv;
        } else if (!lit_float(e, &fv)) {
            Diag *d = ck_err("E0329", e, str_from_cstr("This field is a `float`: write its range with decimals."));
            if (lit_int(e, &iv)) diag_add_fix(d, (int)e->file, e->end, e->end, str_from_cstr(".0"));
            return 0;
        }
        if (k) *hi = fv;
        else *lo = fv;
    }
    if (*lo > *hi) {
        bad(a, "The lower bound is above the upper one.", 0);
        return 0;
    }
    return 1;
}

static void check_default(Node *a, Node *field, Type *ft)
{
    if (a->b.v->n != 1) {
        bad(a, "`@default` takes one value.", 0);
        return;
    }
    Node *e = vec_get(a->b.v, 0);
    long iv = 0;
    double fv = 0;
    int ok = 0;
    switch (ft->kind) {
    case TY_INT: ok = lit_int(e, &iv), fv = (double)iv; break;
    case TY_FLOAT: ok = lit_float(e, &fv); break;
    case TY_BOOL: ok = e->kind == N_BOOL; break;
    case TY_STR: ok = e->kind == N_STR; break;
    case TY_LIST:
        if (e->kind == N_LIST && !e->a.v->n) return;
        bad(e, "A list field's default can only be `[]`.", 0);
        return;
    case TY_VARIANT:
        if (ty_enum_like(ft) && e->kind == N_VCASE && !e->c.n && e->a.n && e->b.n &&
            str_eq(e->a.n->a.s, ft->decl.n->a.n->a.s) && member_index(ft->decl.n->b.v, e->b.n->a.s) >= 0)
            return;
        bad(e, "The default of this field is one of `$1`'s cases, written `$1.case`.", ft->decl.n->a.n->a.s);
        return;
    default:
        bad(a, "A field of this type cannot have a default: make it a list of at most one item to make it optional.", 0);
        return;
    }
    if (!ok) {
        bad(e, "The default must be a literal of the field's type `$1`.", ty_name(ft));
        return;
    }
    Node *r = find_annot(field->c.v, ft->kind == TY_STR ? "size" : "range");
    double lo, hi;
    if (r && r->b.v->n == 2 && bounds(r, ft, ft->kind == TY_STR, &lo, &hi)) {
        double v = ft->kind == TY_STR ? (double)utf8_chars(e->a.s) : fv;
        if (v < lo || v > hi) bad(e, "The default breaks the field's own limits.", 0);
    }
}

/* The annotations of one record field. */
static void check_field_annots(Node *field, Type *ft)
{
    Vec *v = field->c.v;
    for (long i = 0; i < v->n; i++) { /* spec 031: constants in the arguments become their values */
        Node *a = vec_get(v, i);
        for (long j = 0; a->b.v && j < a->b.v->n; j++) const_literal_any(vec_get(a->b.v, j));
    }
    for (long i = 0; i < v->n; i++) {
        Node *a = vec_get(v, i);
        if (!a->a.n) continue;
        Str *name = a->a.n->a.s;
        int known = -1;
        for (int k = 0; annot_names[k]; k++)
            if (str_eq_cstr(name, annot_names[k])) known = k;
        if (known < 0) {
            Diag *d = ck_err("E0329", a->a.n, msg("`@$1` is not a field annotation: use @size, @range, @pattern, @default or @header.", name, 0, 0));
            long best = 3;
            char *fix = 0;
            for (int k = 0; annot_names[k]; k++) {
                long dist = name_distance(name, str_from_cstr(annot_names[k]));
                if (dist < best) best = dist, fix = annot_names[k];
            }
            if (fix) diag_add_fix(d, (int)a->a.n->file, a->a.n->start, a->a.n->end, str_from_cstr(fix));
            continue;
        }
        for (long j = 0; j < i; j++) {
            Node *p = vec_get(v, j);
            if (p->a.n && str_eq(p->a.n->a.s, name)) {
                bad(a, "`@$1` is given twice for this field.", name);
                goto next;
            }
        }
        if (ty_bad(ft)) continue;
        double lo, hi;
        switch (known) {
        case 0: /* size */
            if (ft->kind != TY_STR && ft->kind != TY_LIST && ft->kind != TY_MAP)
                bad(a, "`@size` limits a `str`, list or map; this field is `$1`.", ty_name(ft));
            else
                bounds(a, ft, 1, &lo, &hi);
            break;
        case 1: /* range */
            if (ft->kind != TY_INT && ft->kind != TY_FLOAT)
                bad(a, "`@range` limits an `int` or `float`; this field is `$1`.", ty_name(ft));
            else
                bounds(a, ft, 0, &lo, &hi);
            break;
        case 2: /* pattern */
            if (ft->kind != TY_STR) {
                bad(a, "`@pattern` limits a `str`; this field is `$1`.", ty_name(ft));
            } else if (a->b.v->n != 1 || ((Node *)vec_get(a->b.v, 0))->kind != N_STR) {
                bad(a, "`@pattern` takes one string literal or constant.", 0);
            } else {
                Node *lit = vec_get(a->b.v, 0);
                if (lit->flags & NF_CONST) diag_const_node = lit; /* spec 031 */
                check_regex_literal(lit);
                diag_const_node = 0;
            }
            break;
        case 3: check_default(a, field, ft); break;
        case 4: /* header */
            if (!is_scalar(ft))
                bad(a, "`@header` binds a `str`, `int`, `float` or `bool`; this field is `$1`.", ty_name(ft));
            else if (a->b.v->n != 1 || ((Node *)vec_get(a->b.v, 0))->kind != N_STR)
                bad(a, "`@header` takes the header's name as a string literal or constant.", 0);
            break;
        }
    next:;
    }
}

void check_field_annotations(Node *record)
{
    Vec *members = record->b.v;
    for (long i = 0; i < members->n; i++) {
        Node *m = vec_get(members, i);
        if (m->kind == N_FIELD_DECL && m->c.v && m->b.n) check_field_annots(m, m->b.n->type ? m->b.n->type : resolve_type(m->b.n));
    }
}
