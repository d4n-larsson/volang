/* Constants (spec 031): `const name = expr;` items. The checker works out each value once, from
 * literals, other constants and operators, with the run-time rules (vo_int_op in vo.h for `int`, C
 * `double` operations for `float`, values_equal and str_cmp as the VM uses them), and folds every use of
 * a constant into a literal node of its value (NF_CONST, N_B the declaration). The compiler, the effects
 * checks and every check that reads a literal then see an ordinary literal (research R2, R3).
 *
 * An N_CONST's state is in N_D (CS_*); N_E is 1 once something refers to it (W0202); its value is its
 * expression N_C, folded in place, and its type the declaration's `type`. */
#include "vo.h"

enum { CS_NEW, CS_BUSY, CS_DONE }; /* and CS_BAD (vo.h) */

#define M_VALUE "A constant's value is made of literals, constants and operators: "

static Vec *cs_stack; /* the constants being worked out, innermost last (E0213) */

static int is_literal(Node *n)
{
    return n && (n->kind == N_INT || n->kind == N_FLOAT || n->kind == N_STR || n->kind == N_BOOL);
}

/* The four types a constant may have. */
static int const_type_ok(Type *t)
{
    return t == t_int || t == t_float || t == t_bool || t == t_str;
}

/* E0212 for anything in a constant's value but literals, names and operators; 1 when there is none.
 * Undefined names are left to the checker (E0201 with its suggestion). */
static int value_ok(Node *e)
{
    if (!e) return 1;
    switch (e->kind) {
    case N_INT:
    case N_FLOAT:
    case N_STR:
    case N_BOOL: return 1;
    case N_IDENT: {
        Sym *s = scope_find(e->a.s);
        if (!s || s->kind == SYM_CONST) return 1;
        ck_err1("E0212", e, M_VALUE "`$1` is not a constant.", e->a.s);
        return 0;
    }
    case N_BINARY: {
        int a = value_ok(e->a.n);
        int b = value_ok(e->b.n);
        return a && b;
    }
    case N_UNARY: return value_ok(e->a.n);
    case N_CALL:
        if (e->a.n && e->a.n->a.s) {
            ck_err1("E0212", e, M_VALUE "it cannot call `$1`.", e->a.n->a.s);
            return 0;
        }
        break;
    default: break;
    }
    ck_err("E0212", e, str_from_cstr(M_VALUE "this is not allowed here."));
    return 0;
}

/* e becomes the literal of kind with this value (in place: its span and type stay). */
static void set_literal(Node *e, int kind, long value)
{
    e->kind = (uint16_t)kind;
    e->a.i = value;
    e->b.p = e->c.p = e->d.p = e->e.p = 0;
    e->flags = (uint16_t)((e->flags & NF_PAREN) | NF_CONST);
}

static double as_double(long bits)
{
    union {
        long i;
        double d;
    } u = {bits};
    return u.d;
}

static long as_bits(double d)
{
    union {
        double d;
        long i;
    } u = {d};
    return u.i;
}

/* E0333 at e for vo_int_op's status (1 overflow, 2 division by zero) when report is set. */
static void arith_error(Node *e, int status, int report)
{
    if (report) ck_err("E0333", e, str_from_cstr(status == 2 ? "This divides by zero." : "This overflows `int`."));
}

/* Folds a checked expression whose leaves are literals into one literal, in place; 1 when e is now a
 * literal. Overflow and division by zero do not fold (E0333 when report is set). */
int const_fold(Node *e, int report)
{
    if (!e) return 0;
    if (is_literal(e)) return 1;
    long r;
    if (e->kind == N_UNARY) {
        if (!const_fold(e->a.n, report)) return 0;
        long x = e->a.n->a.i;
        switch (e->d.i) {
        case OP_NOT: set_literal(e, N_BOOL, !x); return 1;
        case OP_NEG_FLOAT: set_literal(e, N_FLOAT, (long)((unsigned long)x ^ (1UL << 63))); return 1;
        case OP_NEG_INT: {
            int st = vo_int_op(OP_NEG_INT, x, 0, &r);
            if (st) {
                arith_error(e, st, report);
                return 0;
            }
            set_literal(e, N_INT, r);
            return 1;
        }
        default: return 0;
        }
    }
    if (e->kind != N_BINARY) return 0;
    int fa = const_fold(e->a.n, report);
    int fb = const_fold(e->b.n, report);
    if (!fa || !fb) return 0;
    long op = e->d.i, x = e->a.n->a.i, y = e->b.n->a.i;
    double dx = as_double(x), dy = as_double(y);
    switch (op) {
    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_DIV_INT:
    case OP_REM_INT: {
        int st = vo_int_op(op, x, y, &r);
        if (st) {
            arith_error(e, st, report);
            return 0;
        }
        set_literal(e, N_INT, r);
        return 1;
    }
    case OP_ADD_FLOAT: set_literal(e, N_FLOAT, as_bits(dx + dy)); return 1;
    case OP_SUB_FLOAT: set_literal(e, N_FLOAT, as_bits(dx - dy)); return 1;
    case OP_MUL_FLOAT: set_literal(e, N_FLOAT, as_bits(dx * dy)); return 1;
    case OP_DIV_FLOAT: set_literal(e, N_FLOAT, as_bits(dx / dy)); return 1;
    case OP_CONCAT: {
        perm_begin(); /* the tree outlives every run and test */
        Str *s = str_concat(e->a.n->a.s, e->b.n->a.s);
        perm_end();
        set_literal(e, N_STR, (long)s);
        return 1;
    }
    case OP_EQ:
    case OP_NE: {
        long eq = values_equal(x, y, e->a.n->type);
        set_literal(e, N_BOOL, op == OP_EQ ? eq : !eq);
        return 1;
    }
    case OP_LT_INT: set_literal(e, N_BOOL, x < y); return 1;
    case OP_LE_INT: set_literal(e, N_BOOL, x <= y); return 1;
    case OP_GT_INT: set_literal(e, N_BOOL, x > y); return 1;
    case OP_GE_INT: set_literal(e, N_BOOL, x >= y); return 1;
    case OP_LT_FLOAT: set_literal(e, N_BOOL, dx < dy); return 1;
    case OP_LE_FLOAT: set_literal(e, N_BOOL, dx <= dy); return 1;
    case OP_GT_FLOAT: set_literal(e, N_BOOL, dy < dx); return 1; /* as the VM: NaN compares false */
    case OP_GE_FLOAT: set_literal(e, N_BOOL, dy <= dx); return 1;
    case OP_LT_STR: set_literal(e, N_BOOL, str_cmp(e->a.n->a.s, e->b.n->a.s) < 0); return 1;
    case OP_LE_STR: set_literal(e, N_BOOL, str_cmp(e->a.n->a.s, e->b.n->a.s) <= 0); return 1;
    case OP_GT_STR: set_literal(e, N_BOOL, str_cmp(e->a.n->a.s, e->b.n->a.s) > 0); return 1;
    case OP_GE_STR: set_literal(e, N_BOOL, str_cmp(e->a.n->a.s, e->b.n->a.s) >= 0); return 1;
    case OP_AND: set_literal(e, N_BOOL, x && y); return 1;
    case OP_OR: set_literal(e, N_BOOL, x || y); return 1;
    default: return 0;
    }
}

/* Spec 031, literal positions (scopes, annotations, templates): e, a constant expression of type want,
 * checked and folded in place; 1 when it is now a literal of that type. A node is checked once (its
 * `type` is set after), so its problems are reported once (E0212, E0301, E0333, E0201...). */
int const_literal(Node *e, Type *want)
{
    if (!e) return 0;
    if (e->type) return is_literal(e) && e->type == want; /* checked before */
    if (!is_literal(e) && !value_ok(e)) {
        e->type = t_error;
        return 0;
    }
    Type *t = check_expr(e, want);
    if (ty_bad(t)) return 0;
    if (t != want) {
        expect_type(e, want, t);
        return 0;
    }
    return const_fold(e, 1);
}

/* The same for a position that takes a literal of several types (annotation arguments): a name or an
 * operator expression becomes the literal of its value; literals, lists and cases are left as written.
 * 1 when e is a literal afterwards. */
int const_literal_any(Node *e)
{
    if (!e) return 0;
    if (is_literal(e)) return 1;
    if (e->kind != N_IDENT && e->kind != N_BINARY && e->kind != N_UNARY) return 0;
    if (e->type) return 0; /* checked before, and not a constant */
    if (!value_ok(e)) {
        e->type = t_error;
        return 0;
    }
    if (ty_bad(check_expr(e, 0))) return 0;
    return const_fold(e, 1);
}

/* Whether e is made of literals, constants and operators only (nothing is reported). */
static int const_shaped(Node *e)
{
    if (!e) return 0;
    if (is_literal(e)) return 1;
    if (e->kind == N_IDENT) {
        Sym *s = scope_find(e->a.s);
        return s && s->kind == SYM_CONST;
    }
    if (e->kind == N_BINARY) return const_shaped(e->a.n) && const_shaped(e->b.n);
    if (e->kind == N_UNARY) return const_shaped(e->a.n);
    return 0;
}

/* A template, pattern, statement or route argument (checked or not): a constant expression becomes the
 * `str` literal of its value; 1 when e is a `str` literal afterwards. Anything else is left for the
 * caller's "must be a literal" error. */
int const_template(Node *e)
{
    if (!e) return 0;
    if (e->kind == N_STR) return 1;
    if (!const_shaped(e)) return 0;
    if (e->type) return e->type == t_str && const_fold(e, 1);
    return const_literal(e, t_str);
}

/* The type of the constant decl, working out its value first if needed; t_error when it is in error.
 * at: the use that asks (0 for the pass over a file's items); a use marks the constant used. */
Type *const_eval(Node *decl, Node *at)
{
    if (at) decl->e.i = 1;
    if (decl->d.i == CS_DONE) return decl->type;
    if (decl->d.i == CS_BAD) return t_error;
    if (decl->d.i == CS_BUSY) { /* a cycle: E0213 at the use that closes it */
        SB *sb = sb_new();
        long from = 0;
        while (from < cs_stack->n && cs_stack->data[from].n != decl) from++;
        for (long i = from; i < cs_stack->n; i++) {
            sb_str(sb, ((Node *)vec_get(cs_stack, i))->a.n->a.s);
            sb_cstr(sb, " -> ");
        }
        sb_str(sb, decl->a.n->a.s);
        ck_err1("E0213", at ? at : decl->a.n, "Constants are defined from each other in a cycle: $1.", sb_finish(sb));
        decl->d.i = CS_BAD;
        return t_error;
    }
    if (!cs_stack) cs_stack = vec_new();
    decl->d.i = CS_BUSY;
    vec_push(cs_stack, decl);
    int ok = decl->a.n && decl->c.n;
    Type *declared = 0;
    if (ok && decl->b.n) {
        declared = resolve_type(decl->b.n);
        if (!ty_bad(declared) && !const_type_ok(declared)) {
            ck_err1("E0332", decl->b.n, "A constant is an `int`, `float`, `bool` or `str`, found `$1`.", ty_name(declared));
            ok = 0;
        }
    }
    Type *t = t_error;
    if (ok) ok = value_ok(decl->c.n);
    if (ok) {
        t = check_expr(decl->c.n, declared);
        if (declared && !ty_bad(declared)) {
            if (!ty_bad(t) && t != declared) ok = 0;
            expect_type(decl->c.n, declared, t); /* E0301, as for `let` */
            t = declared;
        }
        if (ok && !ty_bad(t) && !const_type_ok(t)) {
            ck_err1("E0332", decl->c.n, "A constant is an `int`, `float`, `bool` or `str`, found `$1`.", ty_name(t));
            ok = 0;
        }
        if (ty_bad(t) || !const_fold(decl->c.n, 1)) ok = 0;
    }
    cs_stack->n--;
    if (decl->d.i == CS_BUSY) decl->d.i = ok ? CS_DONE : CS_BAD;
    decl->type = decl->d.i == CS_DONE ? t : t_error;
    return decl->type;
}

/* A use of the constant decl (checked: const_eval gave its type) becomes the literal of its value, in
 * place; its span stays, so diagnostics and `vo query` still point at the name. */
void const_use(Node *node, Node *decl)
{
    Node *v = decl->c.n;
    node->kind = v->kind;
    node->a = v->a;
    node->b.n = decl;
    node->c.p = node->d.p = node->e.p = 0;
    node->flags |= NF_CONST;
}

/* Works out every constant of a file, before effect sets and signatures that may name them. */
void const_check_all(Vec *items)
{
    cs_stack = vec_new();
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_CONST) const_eval(it, 0);
    }
}

/* W0202 for each private constant nothing refers to, with a fix that removes its declaration. */
void const_report_unused(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind != N_CONST || (it->flags & NF_EXPORT) || it->e.i || !it->a.n || it->d.i == CS_BAD) continue;
        if (naming_rename(it->a.n, 0)) continue; /* E0207 already asks for a change at its name */
        Diag *d = ck_warn("W0202", it->a.n, msg("`$1` is never used.", it->a.n->a.s, 0, 0));
        FileRec *f = file_get((int)it->file);
        long end = it->end;
        if (end < f->len && f->text[end] == '\n') end++;
        diag_add_fix(d, (int)it->file, it->start, end, str_from_cstr(""));
    }
}
