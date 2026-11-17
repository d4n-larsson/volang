/* Checker: types, scopes, declarations, statements and control flow (check_expr.s: expressions;
 * ported from src/check.s, spec 008).
 *
 * Types are interned (ty_make), so equal types are the same pointer. Scopes are one chain of
 * symbols through SY_NEXT from ck_scope; leaving a scope pops back to a saved symbol. */
#include "vo.h"

Type *t_int;
Type *t_float;
Type *t_bool;
Type *t_str;
Type *t_void;
Type *t_error;
static Vec *type_tab; /* interned types */
Sym *ck_scope;        /* innermost symbol */
Type *ck_ret;         /* return type of the function being checked (0 in tests) */
long ck_in_test;
long ck_test_checks; /* spec 018: expect/expect_eq calls in the test being checked (W0602) */
long ck_test_holes;  /* and its holes */
long ck_loops;
long ck_locals;
long ck_mode;
long ck_broke; /* a `break` was seen in the innermost loop */
Node *ck_fn;
Vec *ck_blk;     /* the statements of the innermost block being checked */
long ck_blk_idx; /* the index after the statement being checked */

#define B_EXIT 9 /* builtin id of exit() */
#define M_UNHANDLED "The `$1` returned here is ignored. Handle it with `try` or `match`."

void ck_init(void)
{
    if (type_tab) return;
    type_tab = vec_new();
    t_int = ty_basic(TY_INT);
    t_float = ty_basic(TY_FLOAT);
    t_bool = ty_basic(TY_BOOL);
    t_str = ty_basic(TY_STR);
    t_void = ty_basic(TY_VOID);
    t_error = ty_basic(TY_ERROR);
}

Type *ty_basic(long kind)
{
    return ty_make(kind, 0, 0, 0);
}

/* The interned type with these fields (also used while programs run: generic instances). */
Type *ty_make(long kind, void *a, void *b, void *decl)
{
    for (long i = 0; i < type_tab->n; i++) {
        Type *t = type_tab->data[i].t;
        if (t->kind == kind && t->a.p == a && t->b.p == b && t->decl.p == decl) return t;
    }
    GC_ROOT(&a);
    GC_ROOT(&b);
    GC_ROOT(&decl);
    Type *t = alloc(TYPE_SIZE);
    t->kind = kind;
    t->a.p = a;
    t->b.p = b;
    t->decl.p = decl;
    GC_ROOT(&t);
    vec_push(type_tab, t);
    GC_UNROOT(4);
    return t;
}

Type *ty_list(Type *elem)
{
    return ty_make(TY_LIST, elem, 0, 0);
}

Type *ty_result(Type *inner)
{
    return ty_make(TY_RESULT, inner, 0, 0);
}

void ty_name_into(SB *sb, Type *t)
{
    switch (t->kind) {
    case TY_INT: sb_cstr(sb, "int"); return;
    case TY_FLOAT: sb_cstr(sb, "float"); return;
    case TY_BOOL: sb_cstr(sb, "bool"); return;
    case TY_STR: sb_cstr(sb, "str"); return;
    case TY_VOID: sb_cstr(sb, "void"); return;
    case TY_LIST:
        sb_cstr(sb, "list[");
        ty_name_into(sb, t->a.t);
        break;
    case TY_MAP:
        sb_cstr(sb, "map[");
        ty_name_into(sb, t->a.t);
        sb_cstr(sb, ", ");
        ty_name_into(sb, t->b.t);
        break;
    case TY_RESULT:
        sb_cstr(sb, "result[");
        ty_name_into(sb, t->a.t);
        break;
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: /* spec 014 */
        sb_cstr(sb, t->kind == TY_OMAP ? "ordered_map[" : t->kind == TY_OSET ? "ordered_set[" : t->kind == TY_PQUEUE ? "priority_queue[" : "deque[");
        ty_name_into(sb, t->a.t);
        if (t->b.t) {
            sb_cstr(sb, ", ");
            ty_name_into(sb, t->b.t);
        }
        break;
    case TY_RECORD:
    case TY_VARIANT:
        sb_str(sb, t->decl.n->a.n->a.s);
        if (t->b.v) ty_args_into(sb, t->b.v); /* an instance: Name[args] */
        return;
    case TY_FUNC: ty_func_name_into(sb, t); return;
    case TY_PARAM: sb_str(sb, ty_param_name(t)); return;
    default: sb_cstr(sb, "<error>"); return;
    }
    sb_byte(sb, ']');
}

Str *ty_name(Type *t)
{
    SB *sb = sb_new();
    ty_name_into(sb, t);
    return sb_finish(sb);
}

int ty_is(Type *t, long kind)
{
    return t && t->kind == kind;
}

/* 1 when the type is the error type (or missing): no further diagnostics. */
int ty_bad(Type *t)
{
    return !t || t->kind == TY_ERROR;
}

Diag *ck_err(char *code, Node *node, Str *message)
{
    return diag_add(code, SEV_ERROR, (int)node->file, node->start, node->end, message);
}

Diag *ck_warn(char *code, Node *node, Str *message)
{
    return diag_add(code, SEV_WARNING, (int)node->file, node->start, node->end, message);
}

/* ck_err with a message built from a template and one argument. */
Diag *ck_err1(char *code, Node *node, char *tmpl, Str *arg)
{
    return ck_err(code, node, msg(tmpl, arg, 0, 0));
}

/* E0301 "Expected `X`, found `Y`." with expected/found set. */
Diag *ck_mismatch(Node *node, Type *expected, Type *found)
{
    Str *e = ty_name(expected);
    Str *f = ty_name(found);
    Diag *d = ck_err("E0301", node, msg("Expected `$1`, found `$2`.", e, f, 0));
    diag_set_ef(d, e, f);
    return d;
}

Sym *sym_new(Str *name, long kind, Node *decl, Type *type, Node *name_node)
{
    Sym *s = alloc(SYM_SIZE);
    s->name = name;
    s->kind = kind;
    s->decl = decl;
    s->type = type;
    s->node = name_node;
    q_note_decl(s); /* spec 006 reference log */
    return s;
}

void scope_add(Sym *s)
{
    s->next = ck_scope;
    ck_scope = s;
}

/* The innermost symbol with this name, or 0. */
Sym *scope_find(Str *name)
{
    for (Sym *s = ck_scope; s; s = s->next)
        if (str_eq(s->name, name)) return s;
    return 0;
}

/* Pops symbols back to `saved`, warning (W0201) about locals that were never used. */
void scope_leave(Sym *saved)
{
    for (Sym *s = ck_scope; s && s != saved; s = s->next) {
        if (s->kind != SYM_LOCAL || (s->flags & SYF_USED)) continue;
        if (s->slot < 0) continue; /* parameters (slot stored negative-flagged) are not reported */
        Fx *fx = (Fx *)ck_fx;      /* a function with a hole is unfinished: no W0201 */
        if (fx && fx->holes) continue;
        if (!s->node) continue;
        ck_warn("W0201", s->node, msg("`$1` is never used.", s->name, 0, 0));
    }
    ck_scope = saved;
}

/* The name converted to snake_case (upper = 0) or UpperCamel (upper = 1). */
Str *convert_name(Str *name, int upper)
{
    char *p = name->data;
    long n = name->len;
    SB *sb = sb_new();
    int start = 1; /* at a word start (for UpperCamel) */
    for (; n; n--) {
        int c = (unsigned char)*p++;
        if (upper) { /* drop '_', capitalize word starts */
            if (c == '_') {
                start = 1;
                continue;
            }
            if (start && c >= 'a' && c <= 'z') c -= 32;
            start = 0;
        } else if (c >= 'A' && c <= 'Z') { /* '_' before inner capitals, lower everything */
            c += 32;
            if (sb->len && sb->ptr[sb->len - 1] != '_') sb_byte(sb, '_');
        }
        sb_byte(sb, c);
    }
    return sb_finish(sb);
}

/* E0207 when a name breaks the convention, with the converted name as a fix. */
void check_naming(Node *name_node, int upper)
{
    if (!name_node) return;
    Str *name = name_node->a.s;
    Str *conv = convert_name(name, upper);
    if (str_eq(name, conv)) return;
    char *tmpl = upper ? "`$1` should be UpperCamel: `$2`." : "`$1` should be snake_case: `$2`.";
    Diag *d = ck_err("E0207", name_node, msg(tmpl, name, conv, 0));
    diag_add_fix(d, (int)name_node->file, name_node->start, name_node->end, conv);
}

/* The semantic type of a syntax type (t_error after reporting a problem). */
Type *resolve_type(Node *tn)
{
    if (!tn) return t_error;
    Type *t = t_error;
    switch (tn->a.i) {
    case TY_INT: t = t_int; break;
    case TY_FLOAT: t = t_float; break;
    case TY_BOOL: t = t_bool; break;
    case TY_STR: t = t_str; break;
    case TY_VOID: t = t_void; break;
    case TY_LIST: t = ty_list(resolve_type(tn->b.n)); break;
    case TY_MAP: {
        Type *k = resolve_type(tn->b.n);
        Type *v = resolve_type(tn->c.n);
        if (!ty_bad(k) && !ty_is(k, TY_INT) && !ty_is(k, TY_STR)) {
            ck_err1("E0309", tn->b.n, "Map keys must be `int` or `str`, found `$1`.", ty_name(k));
            break;
        }
        t = ty_make(TY_MAP, k, v, 0);
        break;
    }
    case TY_RESULT: t = ty_result(resolve_type(tn->b.n)); break;
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: { /* spec 014: the built-in collections */
        Type *k = resolve_type(tn->b.n);
        GC_ROOT(&k);
        Type *v = tn->c.n ? resolve_type(tn->c.n) : 0;
        t = ty_make(tn->a.i, k, v, 0);
        GC_UNROOT(1);
        break;
    }
    case TY_NAMED: {
        Node *nm = tn->b.n;
        if (!nm) break;
        Sym *s = scope_find(nm->a.s);
        if (!s) {
            report_undefined(nm->a.s, nm);
            break;
        }
        if (is_placeholder(s)) break;
        q_note_sym(s, nm, RR_TYPE); /* spec 006 reference log */
        int kind;
        if (s->kind == SYM_RECORD) {
            kind = TY_RECORD;
        } else if (s->kind == SYM_VARIANT) {
            kind = TY_VARIANT;
        } else if (s->kind == SYM_TPARAM) { /* a type parameter takes no arguments */
            t = tn->c.v ? ty_argn_error(tn, nm->a.s, 0, tn->c.v->n) : s->type;
            break;
        } else {
            ck_err1("E0201", nm, "`$1` is not a type.", nm->a.s);
            break;
        }
        rename_use(s, nm);
        t = resolve_instance(tn, s->decl, kind, tn->c.p); /* Name or Name[args] */
        break;
    }
    case TY_FUNC: { /* fn(params) -> ret uses effects */
        Vec *params = vec_new();
        for (long i = 0; i < tn->b.v->n; i++) vec_push(params, resolve_type(vec_get(tn->b.v, i)));
        Type *ret = t_void;
        if (tn->c.n) ret = resolve_type(tn->c.n);
        t = ty_func(params, ret, fx_build_set(tn->d.p, 0));
        break;
    }
    default: break;
    }
    tn->type = t;
    return t;
}

/* The type a record or variant declaration defines. */
Type *decl_type(Node *decl)
{
    return ty_make(decl->kind == N_RECORD ? TY_RECORD : TY_VARIANT, 0, 0, decl);
}

/* Defines a local in the current scope (E0203 when the name is already visible). */
Sym *new_local(Node *name_node, Type *type, int mutable_)
{
    check_naming(name_node, 0);
    Sym *prev = scope_find(name_node->a.s);
    if (prev && !is_placeholder(prev)) {
        Diag *d = ck_err1("E0203", name_node, "`$1` is already defined in an outer scope.", name_node->a.s);
        if (prev->node) {
            Str *m = msg("`$1` is defined here.", name_node->a.s, 0, 0);
            Node *pn = prev->node;
            diag_add_related(d, (int)pn->file, pn->start, pn->end, m);
        }
    }
    Sym *s = sym_new(name_node->a.s, SYM_LOCAL, 0, type, name_node);
    s->flags = mutable_ ? SYF_MUT : 0;
    s->rename = naming_rename(name_node, 0);
    s->slot = ck_locals++;
    s->effects.i = ck_depth; /* SY_DEPTH */
    scope_add(s);
    return s;
}

/* Reports E0301 when found does not match expected (errors match anything). 1 = fine. */
int expect_type(Node *node, Type *expected, Type *found)
{
    if (expected == found || ty_bad(expected) || ty_bad(found)) return 1;
    int fits = ty_func_fits(found, expected); /* a function value: wider effects is E0502 */
    if (!fits) return 1;
    if (fits == 2) {
        ty_effects_error(node, expected, found);
        return 0;
    }
    Diag *d = ck_mismatch(node, expected, found);
    suggest_conversion(d, expected, found, node);
    return 0;
}

/* Checks the statements of a block in a new scope; sets NF_TERMINATES on the block. */
void check_block(Node *block)
{
    Sym *saved = ck_scope;
    Vec *stmts = block->a.v;
    int term = 0; /* 1 = later statements are unreachable, 2 = W0401 given */
    for (long i = 0; i < stmts->n;) {
        Node *st = vec_get(stmts, i++);
        if (st->kind == N_COMMENT) continue;
        if (term == 1) { /* first unreachable statement: W0401 once */
            ck_warn("W0401", st, str_from_cstr("This statement can never run."));
            term = 2;
        }
        ck_blk = stmts; /* where the statement is (E0310 fixes look at the ones after it) */
        ck_blk_idx = i;
        check_stmt(st);
        if ((st->flags & NF_TERMINATES) && !term) term = 1;
    }
    if (term) block->flags |= NF_TERMINATES;
    scope_leave(saved);
}

void mark_terminates(Node *node)
{
    node->flags |= NF_TERMINATES;
}

int terminates(Node *node)
{
    return node && (node->flags & NF_TERMINATES);
}

static void check_let(Node *st)
{
    Type *declared = 0;
    if (st->b.n) declared = resolve_type(st->b.n);
    Type *t = check_expr(st->c.n, declared);
    if (declared) {
        expect_type(st->c.n, declared, t);
        t = declared;
    }
    if (ty_is(t, TY_VOID)) {
        ck_err("E0301", st->c.n, str_from_cstr("Expected a value, found `void`."));
        t = t_error;
    }
    st->type = t;
    if (st->flags & NF_DISCARD) { /* `let _ = e;`: a result may not be discarded */
        st->e.i = -1;
        if (ty_is(t, TY_RESULT)) ck_err1("E0404", st->c.n, M_UNHANDLED, ty_name(t));
        return;
    }
    if (!st->a.n) return;
    Sym *s = new_local(st->a.n, t, st->flags & NF_MUT);
    st->e.i = s->slot;
    s->decl = st;
}

static void check_set(Node *st)
{
    Node *place = st->a.n;
    if (place->kind == N_IDENT) {
        Sym *s = scope_find(place->a.s);
        if (s && s->kind == SYM_LOCAL && !(s->flags & SYF_MUT)) { /* a `let` or a parameter */
            Node *decl = s->decl;
            int is_let = decl && decl->kind == N_LET;
            char *tmpl = is_let ? "`$1` is declared with `let` and cannot be changed. Declare it with `var`."
                                : "`$1` is a parameter and cannot be changed.";
            Diag *d = ck_err1("E0208", place, tmpl, place->a.s);
            if (is_let) diag_add_fix(d, (int)decl->file, decl->start, decl->start + 3, str_from_cstr("var"));
        }
    }
    Type *pt = check_place(place, 0);
    Type *vt = check_expr(st->b.n, pt);
    expect_type(st->b.n, pt, vt);
}

static void check_for(Node *st)
{
    Sym *saved = ck_scope;
    Type *it = check_expr(st->b.n, 0);
    Type *elem;
    if (st->c.n) { /* range: both bounds int */
        expect_type(st->b.n, t_int, it);
        Type *hi = check_expr(st->c.n, t_int);
        expect_type(st->c.n, t_int, hi);
        elem = t_int;
    } else if (ty_is(it, TY_LIST) || ty_is(it, TY_OSET) || ty_is(it, TY_PQUEUE) || ty_is(it, TY_DEQUE)) {
        elem = it->a.t; /* spec 014: collections iterate over a snapshot (a map: `for k in keys(m)`) */
    } else {
        elem = t_error;
        if (!ty_bad(it))
            ck_err1("E0301", st->b.n, "`for ... in` needs a list or a range `a..b`, found `$1`.", ty_name(it));
    }
    if (st->a.n) {
        Sym *s = new_local(st->a.n, elem, 0);
        st->e.i = s->slot;
    } else {
        st->e.i = ck_locals++; /* `for _ in`: a slot for the counter, no name */
    }
    ck_loops++;
    long broke = ck_broke;
    check_block(st->d.n);
    ck_broke = broke;
    ck_loops--;
    scope_leave(saved);
}

static void check_return(Node *st)
{
    mark_terminates(st);
    Type *rt = ck_ret ? ck_ret : t_void; /* 0 in tests: like void */
    if (st->a.n) {
        Type *t = check_expr(st->a.n, rt);
        if (!ty_is(rt, TY_VOID)) {
            expect_type(st->a.n, rt, t);
        } else if (!ty_bad(t) && ck_fn) {
            ck_err1("E0301", st->a.n, "`$1` returns nothing, but a value is returned here.", ck_fn->a.n->a.s);
        }
        return;
    }
    if (ty_is(rt, TY_VOID) || ty_bad(rt)) return; /* `return;` where a value is needed */
    Str *name = ck_fn->a.n->a.s;
    Str *tn = ty_name(rt);
    ck_err("E0301", st, msg("`$1` must return `$2`.", name, tn, 0));
}

/* A call or try statement: exit() and todo() never return; an unused value or result is an error. */
static void check_expr_stmt(Node *st)
{
    Node *e = st->a.n;
    ck_stmt_call = e;
    Type *t = check_expr(e, 0);
    ck_stmt_call = 0;
    if (ty_bad(t)) return;
    int is_try = e->kind == N_TRY;
    if (e->kind == N_CALL) {
        if (e->d.i == B_TODO) {
            mark_terminates(st);
            return;
        }
        if (e->d.i == B_EXIT) mark_terminates(st);
    }
    if (ty_is(t, TY_VOID)) return;
    if (!is_try && ty_is(t, TY_RESULT)) { /* an ignored result: E0404 (fix: try) */
        Diag *d = ck_err1("E0404", e, M_UNHANDLED, ty_name(t));
        if (ck_ret && ty_is(ck_ret, TY_RESULT))
            diag_add_fix(d, (int)e->file, e->start, e->start, str_from_cstr("try "));
        return;
    }
    Diag *d = ck_err1("E0405", e, "The `$1` returned by this call is ignored. Use it or write `let _ = ...`.",
                      ty_name(t)); /* an ignored value (fix: let _ = ) */
    diag_add_fix(d, (int)e->file, e->start, e->start, str_from_cstr("let _ = "));
}

void check_stmt(Node *st)
{
    ck_cur_stmt = st;
    switch (st->kind) {
    case N_LET: check_let(st); break;
    case N_SET: check_set(st); break;
    case N_IF:
        check_cond(st->a.n);
        check_block(st->b.n);
        if (!st->c.n) break;
        if (st->c.n->kind == N_IF)
            check_stmt(st->c.n);
        else
            check_block(st->c.n);
        if (terminates(st->b.n) && terminates(st->c.n)) mark_terminates(st);
        break;
    case N_WHILE: {
        check_cond(st->a.n);
        long saved = ck_broke;
        ck_broke = 0;
        ck_loops++;
        check_block(st->b.n);
        ck_loops--;
        long broke = ck_broke;
        ck_broke = saved;
        Node *c = st->a.n; /* `while true` without break never falls through */
        if (!broke && c->kind == N_BOOL && c->a.i) mark_terminates(st);
        break;
    }
    case N_FOR: check_for(st); break;
    case N_MATCH: check_match(st); break;
    case N_RETURN: check_return(st); break;
    case N_BREAK:
    case N_CONTINUE: {
        char *kw = "continue";
        if (st->kind == N_BREAK) {
            ck_broke = 1;
            kw = "break";
        }
        mark_terminates(st);
        if (!ck_loops) ck_err1("E0407", st, "`$1` can only be used inside a loop.", str_from_cstr(kw));
        break;
    }
    case N_EXPR_STMT: check_expr_stmt(st); break;
    default: break; /* N_ERROR, comments */
    }
}

void check_cond(Node *expr)
{
    Type *t = check_expr(expr, t_bool);
    if (ty_is(t, TY_BOOL) || ty_bad(t)) return;
    ck_err1("E0301", expr, "A condition must be `bool`, found `$1`.", ty_name(t));
}

void check_match(Node *match)
{
    Type *st = check_expr(match->a.n, 0); /* scrutinee type */
    long ncases = 0;
    if (ty_is(st, TY_RESULT)) {
        ncases = 2;
    } else if (ty_is(st, TY_VARIANT)) {
        ncases = count_members(st->decl.n->b.v);
    } else { /* not matchable: check arm bodies only */
        if (!ty_bad(st))
            ck_err1("E0301", match->a.n, "`match` needs a variant or result value, found `$1`.", ty_name(st));
        st = t_error;
    }
    long *seen = alloc((ncases + 1) * 8);
    Vec *arms = match->b.v;
    int all_end = 1; /* all arms terminate */
    for (long i = 0; i < arms->n; i++) {
        Node *arm = vec_get(arms, i);
        if (arm->kind != N_ARM) continue;
        check_arm(arm, st, seen);
        if (!terminates(arm->c.n)) all_end = 0;
    }
    if (ty_bad(st)) return;
    if (has_unknown_arm(match)) return; /* its fix comes first, no E0401 */
    if (report_missing_cases(match, st, seen, ncases)) return;
    if (all_end) mark_terminates(match);
}

/* Number of non-comment members of a record or variant. */
long count_members(Vec *v)
{
    long n = 0;
    for (long i = 0; i < v->n; i++)
        if (v->data[i].n->kind != N_COMMENT) n++;
    return n;
}

/* The index-th non-comment member. */
Node *member_at(Vec *v, long index)
{
    for (long i = 0; i < v->n; i++) {
        Node *m = v->data[i].n;
        if (m->kind == N_COMMENT) continue;
        if (!index--) return m;
    }
    return 0;
}

/* Index of the member with this name, or -1. */
long member_index(Vec *v, Str *name)
{
    for (long i = 0;; i++) {
        Node *m = member_at(v, i);
        if (!m) return -1;
        if (str_eq(m->a.n->a.s, name)) return i;
    }
}

Str *case_name(Type *scrutinee, long index)
{
    if (scrutinee->kind == TY_RESULT) return str_from_cstr(index ? "err" : "ok");
    return member_at(scrutinee->decl.n->b.v, index)->a.n->a.s;
}

/* Payload type of a case, or 0 when it has none. */
Type *case_payload(Type *scrutinee, long index)
{
    if (scrutinee->kind == TY_RESULT) {
        if (index) return t_str;
        Type *a = scrutinee->a.t;
        return a->kind == TY_VOID ? 0 : a;
    }
    Node *pn = member_at(scrutinee->decl.n->b.v, index)->b.n; /* payload type node */
    if (!pn) return 0;
    return field_type_in(scrutinee, pn->type); /* in an instance, with its type arguments */
}

long case_index(Type *scrutinee, Str *name)
{
    if (scrutinee->kind != TY_RESULT) return member_index(scrutinee->decl.n->b.v, name);
    if (str_eq_cstr(name, "ok")) return 0;
    if (str_eq_cstr(name, "err")) return 1;
    return -1;
}

int str_eq_cstr(Str *str, char *s)
{
    return str_eq_bytes(str, s, cstr_len(s));
}

/* Spec 009: `case ok()` is the success pattern of a `result[void]` (as `ok()` builds it). Elsewhere
 * empty parentheses are E0411; `case ok(_)` and `case ok` on a `result[void]` are W0402. Both fixes
 * rewrite the pattern, from the case name to the arm's block. */
static void check_ok_pattern(Node *arm, Type *scrutinee, long idx)
{
    Node *cn = arm->a.n;
    Type *pt = case_payload(scrutinee, idx);
    int void_ok = scrutinee->kind == TY_RESULT && idx == 0 && scrutinee->a.t->kind == TY_VOID;
    long end = arm->c.n->start; /* the block's `{` */
    if (arm->flags & NF_EMPTY_ARM) {
        if (void_ok) return;
        int value = pt != 0; /* the case carries a value (case_payload: 0 for none, and for ok() of void) */
        Str *form = str_concat(cn->a.s, str_from_cstr(value ? "(_)" : ""));
        Diag *d = ck_err("E0411", cn,
                         msg(value ? "`case $1()` matches only the success of a `result[void]`; write `case $2` to "
                                     "ignore the value."
                                   : "`case $1()` matches only the success of a `result[void]`; `$1` has no value: "
                                     "write `case $2`.",
                             cn->a.s, form, 0));
        diag_add_fix(d, (int)arm->file, cn->start, end, str_concat(form, str_from_cstr(" ")));
    } else if (void_ok && !arm->b.n) { /* `case ok(_)` or `case ok` */
        Diag *d = ck_warn("W0402", cn, str_from_cstr("Match the success of a `result[void]` with `case ok()`, as `ok()` "
                                                     "builds it."));
        diag_add_fix(d, (int)arm->file, cn->start, end, str_from_cstr("ok() "));
    }
}

void check_arm(Node *arm, Type *scrutinee, long *seen)
{
    Sym *saved = ck_scope;
    arm->d.i = -1;
    int silent = ty_bad(scrutinee); /* bind the name without checks (no cascade) */
    if (!silent && arm->a.n) {
        Node *cn = arm->a.n;
        long idx = case_index(scrutinee, cn->a.s);
        if (idx < 0) { /* unknown case */
            Str *tn = ty_name(scrutinee);
            Diag *d = ck_err("E0201", cn, msg("`$1` is not a case of `$2`.", cn->a.s, tn, 0));
            suggest_case(d, scrutinee, cn);
            silent = 1;
        } else {
            arm->d.i = idx;
            if (scrutinee->kind == TY_VARIANT) /* spec 006 reference log: (variant, case); not ok/err */
                q_note((long)scrutinee->decl.p, idx, cn, RR_USE);
            if (seen[idx]) { /* duplicate arm: E0402, fix removes it */
                Diag *d = ck_err1("E0402", cn, "Case `$1` is handled twice.", cn->a.s);
                diag_add_fix(d, (int)arm->file, arm->start, arm->end, str_new(0, 0));
            } else {
                seen[idx] = 1;
            }
            check_ok_pattern(arm, scrutinee, idx);
            if (arm->b.n) { /* payload binding */
                Type *pt = case_payload(scrutinee, idx);
                if (!pt) {
                    ck_err1("E0301", arm->b.n, "Case `$1` has no payload to bind.", cn->a.s);
                    pt = t_error;
                }
                arm->type = pt; /* spec 006: the binding's type (values in runtime errors) */
                Sym *s = new_local(arm->b.n, pt, 0);
                arm->e.i = s->slot;
            }
        }
    }
    if (silent && arm->b.n) {
        Sym *s = new_local(arm->b.n, t_error, 0);
        s->flags = SYF_USED;
    }
    check_block(arm->c.n);
    scope_leave(saved);
}

/* E0401 listing every unhandled case (with a fix inserting their arms). 1 = reported. */
int report_missing_cases(Node *match, Type *type, long *seen, long ncases)
{
    SB *list = sb_new(); /* for the message */
    SB *arms = sb_new(); /* for the fix */
    long missing = 0;
    for (long i = 0; i < ncases; i++) {
        if (seen[i]) continue;
        if (missing) sb_cstr(list, ", ");
        missing++;
        Str *name = case_name(type, i);
        sb_byte(list, '`');
        sb_str(list, name);
        sb_byte(list, '`');
        sb_cstr(arms, "case "); /* "case name(_) {\n}\n" (payload) or "case name {\n}\n" */
        sb_str(arms, name);
        if (case_payload(type, i)) sb_cstr(arms, "(_)");
        sb_cstr(arms, " {\n}\n");
    }
    if (!missing) return 0;
    Str *m = msg("This `match` is missing cases: $1.", sb_finish(list), 0, 0);
    Diag *d = diag_add("E0401", SEV_ERROR, (int)match->file, match->start, match->a.n->end, m);
    long at = (long)match->end - 1; /* insert the arms before the closing brace */
    diag_add_fix(d, (int)match->file, at, at, sb_finish(arms));
    return 1;
}

/* Adds a top-level fn, record or variant to the file scope (E0202 on duplicates). */
void define_item(Node *item)
{
    long kind;
    switch (item->kind) {
    case N_FN: kind = SYM_FN; break;
    case N_RECORD: kind = SYM_RECORD; break;
    case N_VARIANT: kind = SYM_VARIANT; break;
    default: return;
    }
    Node *nn = item->a.n;
    if (!nn) return;
    check_naming(nn, kind != SYM_FN);
    if (builtin_id(nn->a.s)) {
        ck_err1("E0202", nn, "`$1` is a builtin and cannot be redefined.", nn->a.s);
        return;
    }
    Sym *prev = scope_find(nn->a.s);
    if (prev) {
        Diag *d = ck_err1("E0202", nn, "`$1` is already defined.", nn->a.s);
        Str *m = msg("`$1` is first defined here.", nn->a.s, 0, 0);
        Node *pn = prev->node;
        diag_add_related(d, (int)pn->file, pn->start, pn->end, m);
        return;
    }
    Sym *s = sym_new(nn->a.s, kind, item, 0, nn);
    s->rename = naming_rename(nn, kind != SYM_FN); /* a name that breaks the convention: uses too */
    scope_add(s);
}

/* Resolves the types in a declaration's signature, fields or cases (its type parameters in scope). */
void resolve_item(Node *item)
{
    Sym *saved = ck_scope;
    tparams_push(ty_tparams_of(item), item);
    resolve_item_types(item);
    scope_leave(saved);
}

void resolve_item_types(Node *item)
{
    if (item->kind == N_FN) {
        fx_declare(item);
        Vec *params = item->b.v;
        for (long i = 0; i < params->n; i++) {
            Node *p = vec_get(params, i);
            p->type = resolve_type(p->b.n);
        }
        item->type = item->c.n ? resolve_type(item->c.n) : t_void;
    } else if (item->kind == N_RECORD || item->kind == N_VARIANT) {
        Vec *members = item->b.v; /* fields or cases */
        for (long i = 0; i < members->n; i++) {
            Node *m = vec_get(members, i);
            if (m->kind == N_COMMENT) continue;
            check_naming(m->a.n, 0);
            if (m->b.n) resolve_type(m->b.n);
        }
    }
}

/* Each condition is checked as `bool` with its effects going to a scratch record (as for an
 * anonymous function), so the function's own requirement and vo.effects do not change. Any effect
 * is E0603. A hole in a condition counts for the function (FX_HOLES). */
static void check_clause_vec(Vec *conds, Fx *fx)
{
    if (!conds) return;
    for (long i = 0; i < conds->n; i++) {
        Node *c = vec_get(conds, i);
        Fx *scratch = alloc(FX_SIZE);
        scratch->direct = es_new(0);
        void *outer = fx_swap(scratch);
        check_cond(c);
        fx_swap(outer);
        if (scratch->holes) fx->holes = scratch->holes;
        if (es_is_empty(scratch->direct)) continue;
        EffSet *set = scratch->direct; /* name the effects (builtin needs are kept apart in ES_ANY) */
        unsigned long bits = set->mask | set->any;
        if (bits) set = es_new((long)bits);
        ck_err1("E0603", c, "A contract condition must be pure, but this one needs $1.", fx_render(set));
    }
}

/* Contract clauses (spec 005 research R14, R15), after the parameters are bound: each a pure
 * `bool`. `ensures` also sees `ret` (slot FX_RETSLOT), in a scope popped before the body. */
static void check_clauses(Node *fn)
{
    if (!fn->a.n) return;
    Fx *fx = fn->a.n->b.fx;
    if (!fx) return;
    check_clause_vec(fx->requires_, fx);
    Vec *ensures = fx->ensures;
    if (!ensures) return;
    Sym *saved = ck_scope;
    if (!ty_is(fn->type, TY_VOID)) { /* `ret`: a name at the first `ensures` condition */
        Node *ret = alloc(NODE_SIZE);
        ret->kind = N_NAME;
        Node *first = ensures->data[0].n;
        ret->file = first->file;
        ret->start = first->start;
        ret->end = first->end;
        ret->a.s = str_from_cstr("ret");
        Sym *s = new_local(ret, ck_ret, 0);
        s->flags = SYF_USED;
        fx->retslot = s->slot;
    }
    check_clause_vec(ensures, fx);
    scope_leave(saved);
}

void check_fn(Node *fn)
{
    fx_begin(fn);
    ck_fn = fn;
    ck_ret = fn->type;
    ck_in_test = 0;
    ck_loops = 0;
    ck_locals = 0;
    Sym *saved = ck_scope;
    tparams_push(ty_tparams_of(fn), fn); /* type parameters, then parameters */
    Vec *params = fn->b.v;               /* parameters become the first locals */
    for (long i = 0; i < params->n; i++) {
        Node *p = vec_get(params, i);
        Sym *s = new_local(p->a.n, p->type, 0);
        p->e.i = s->slot;
        s->flags = SYF_USED; /* parameters are never reported as unused */
    }
    check_clauses(fn);
    if (fn->d.n) {
        check_block(fn->d.n);
        long k = fn->type->kind; /* E0403: a value-returning function that can fall off its end */
        if (k != TY_VOID && k != TY_ERROR && !terminates(fn->d.n))
            ck_err1("E0403", fn->a.n, "`$1` can reach its end without returning a value.", fn->a.n->a.s);
    }
    fx_end();
    scope_leave(saved);
    fn->e.i = ck_locals;
}

/* A test, or a property (ck_in_test 2): its parameters are the first locals (E0602 when a
 * parameter's type cannot be generated). */
void check_test(Node *test)
{
    fx_declare(test);
    fx_begin(test);
    ck_fn = 0;
    ck_ret = 0;
    ck_in_test = 1;
    ck_loops = 0;
    ck_locals = 0;
    ck_test_checks = 0;
    ck_test_holes = 0;
    Sym *saved = ck_scope;
    int prop = test->flags & NF_PROP;
    if (prop) {
        ck_in_test = 2;
        for (long i = 0; i < test->d.fx->params->n; i++) {
            Node *p = vec_get(test->d.fx->params, i);
            p->type = resolve_type(p->b.n);
            int why = prop_generatable(p->type);
            if (why) {
                char *tmpl = why == 1   ? "Cannot generate values of `$1` for `$2`: it contains a function type."
                             : why == 3 ? "Cannot generate values of `$1` for `$2`: its elements have no built-in order."
                                        : "Cannot generate values of `$1` for `$2`: it has no finite value.";
                Str *tn = ty_name(p->type);
                ck_err("E0602", p, msg(tmpl, tn, p->a.n->a.s, 0));
            }
            Sym *s = new_local(p->a.n, p->type, 0);
            p->e.i = s->slot;
            s->flags = SYF_USED; /* parameters are never reported as unused */
        }
    }
    if (test->b.n) check_block(test->b.n);
    if (!ck_test_checks && !ck_test_holes) { /* spec 018: W0602 at the `test` / `prop` keyword */
        Str *m = msg("Test `$1` checks nothing: it has no `expect` or `expect_eq`, so it can only fail by a "
                     "runtime error.", test->a.s, 0, 0);
        diag_add("W0602", SEV_WARNING, (int)test->file, test->start, test->start + 4, m);
    }
    fx_end();
    if (prop) scope_leave(saved);
    test->e.i = ck_locals;
    ck_in_test = 0;
}

/* E0202 when two tests in a file share a name. */
void check_tests_unique(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *t = vec_get(items, i);
        if (t->kind != N_TEST) continue;
        for (long j = 0; j < i; j++) {
            Node *o = vec_get(items, j);
            if (o->kind != N_TEST || !o->a.s || !t->a.s || !str_eq(o->a.s, t->a.s)) continue;
            ck_err1("E0202", t, "A test named \"$1\" already exists.", t->a.s);
            break;
        }
    }
}

Node *find_main(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_FN && it->a.n && str_eq_cstr(it->a.n->a.s, "main")) return it;
    }
    return 0;
}

/* For `run`: E0408 without main, E0409 for a wrong signature. */
void check_main(int file, Node *fn)
{
    if (!fn) {
        diag_add("E0408", SEV_ERROR, file, 0, 0, str_from_cstr("`vo run` needs a `fn main()`."));
        return;
    }
    if (fn->b.v->n == 0) {
        Type *t = fn->type;
        if (t->kind == TY_VOID || t->kind == TY_ERROR) return;
        if (t->kind == TY_RESULT && t->a.t->kind == TY_VOID) return;
    }
    ck_err("E0409", fn->a.n, str_from_cstr("`main` must be `fn main()` or `fn main() -> result[void]`."));
}

/* Declarations, signatures, then bodies of one parsed file. */
/* E0210 for each record or variant of this file without `export` that the resolved type node of an
 * exported item names (spec 010 research R3); item is that item's name, for the message. */
static void export_type(Node *tn, Node *item)
{
    if (!tn || tn->kind != N_TYPE_NODE) return;
    switch (tn->a.i) {
    case TY_LIST:
    case TY_RESULT: export_type(tn->b.n, item); break;
    case TY_MAP:
        export_type(tn->b.n, item);
        export_type(tn->c.n, item);
        break;
    case TY_FUNC:
        for (long i = 0; i < tn->b.v->n; i++) export_type(vec_get(tn->b.v, i), item);
        export_type(tn->c.n, item);
        break;
    case TY_NAMED: {
        Type *t = tn->type;
        if (t && (t->kind == TY_RECORD || t->kind == TY_VARIANT)) {
            Node *decl = t->decl.n;
            if (decl && decl->file == tn->file && !(decl->flags & NF_EXPORT))
                export_needed("E0210", tn->b.n, decl,
                              msg("`export $1` uses `$2`, which is not exported.", item->a.n->a.s, tn->b.n->a.s, 0));
        }
        if (tn->c.v)
            for (long i = 0; i < tn->c.v->n; i++) export_type(vec_get(tn->c.v, i), item);
        break;
    }
    default: break;
    }
}

/* E0210 over every exported item's parameter, result, field and case types. */
static void check_export_types(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (!(it->flags & NF_EXPORT) || !it->a.n) continue;
        if (it->kind == N_FN) {
            for (long j = 0; j < it->b.v->n; j++) export_type(((Node *)vec_get(it->b.v, j))->b.n, it);
            export_type(it->c.n, it);
        } else { /* fields or cases; comments are skipped */
            for (long j = 0; j < it->b.v->n; j++) {
                Node *m = vec_get(it->b.v, j);
                if (m->kind == N_FIELD_DECL || m->kind == N_CASE_DECL) export_type(m->b.n, it);
            }
        }
    }
}

/* ---- spec 012: annotations ---- */

/* 1 when id is PART(/PART)*, PART one or more of A-Z a-z 0-9 _ -. */
static int spec_id_ok(Str *id)
{
    long part = 0;
    for (long i = 0; i <= id->len; i++) {
        int c = i < id->len ? (unsigned char)id->data[i] : '/';
        if (c == '/') {
            if (!part) return 0;
            part = 0;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                   c == '-') {
            part++;
        } else {
            return 0;
        }
    }
    return 1;
}

/* `@spec(` + the IDs (each once when dedup) + `)`, IDs of every annotation in vs (N_ANNOTATION nodes). */
static Str *spec_text(Vec *anns, int dedup)
{
    SB *sb = sb_new();
    Vec *seen = vec_new();
    sb_cstr(sb, "@spec(");
    for (long a = 0; a < anns->n; a++) {
        Vec *ids = ((Node *)vec_get(anns, a))->b.v;
        for (long i = 0; i < ids->n; i++) {
            Str *s = ((Node *)vec_get(ids, i))->a.s;
            int dup = 0;
            for (long k = 0; dedup && k < seen->n; k++) dup |= str_eq(vec_get(seen, k), s);
            if (dup) continue;
            if (seen->n) sb_cstr(sb, ", ");
            vec_push(seen, s);
            render_str_lit(sb, s);
        }
    }
    sb_byte(sb, ')');
    return sb_finish(sb);
}

/* The annotations in front of each item: E0116 unknown name, E0117 not on a test or prop, E0118 a bad or
 * missing ID, E0119 two @spec on one item, W0101 an ID twice (spec 012 research R2). */
static void check_annotations(Vec *items)
{
    for (long i = 0; i < items->n;) {
        Node *first = vec_get(items, i);
        if (first->kind != N_ANNOTATION) {
            i++;
            continue;
        }
        Vec *specs = vec_new();
        long j = i;
        for (; j < items->n && ((Node *)vec_get(items, j))->kind == N_ANNOTATION; j++) {
            Node *a = vec_get(items, j);
            Str *name = a->a.n->a.s;
            if (!str_eq_cstr(name, "spec")) {
                Diag *d = ck_err1("E0116", a->a.n, "`@$1` is not an annotation: the only one is `@spec`.", name);
                if (name_distance(name, str_from_cstr("spec")) <= 2)
                    diag_add_fix(d, (int)a->file, a->a.n->start, a->a.n->end, str_from_cstr("spec"));
                continue;
            }
            vec_push(specs, a);
            Vec *ids = a->b.v;
            if (!ids->n) ck_err("E0118", a, str_from_cstr("`@spec` needs at least one requirement ID: `@spec(\"FR-001\")`."));
            int repeated = 0;
            for (long k = 0; k < ids->n; k++) {
                Node *id = vec_get(ids, k);
                if (!spec_id_ok(id->a.s))
                    ck_err1("E0118", id,
                            "\"$1\" is not a requirement ID: use letters, digits, `-` and `_`, parts separated by `/` (`FR-003`, `US1/AC2`).",
                            id->a.s);
                for (long m = 0; m < k; m++) repeated |= str_eq(((Node *)vec_get(ids, m))->a.s, id->a.s);
            }
            if (repeated) {
                Vec *one = vec_new();
                vec_push(one, a);
                Diag *d = ck_warn("W0101", a, str_from_cstr("`@spec` names the same ID more than once."));
                diag_add_fix(d, (int)a->file, a->start, a->end, spec_text(one, 1));
            }
        }
        Node *target = j < items->n ? vec_get(items, j) : 0;
        if (target && target->kind != N_TEST)
            for (long k = i; k < j; k++)
                ck_err("E0117", vec_get(items, k),
                       str_from_cstr("`@spec` goes on a `test` or `prop`, on the line right above it."));
        if (specs->n > 1) { /* one fix: every @spec of the item merged into the first's place */
            Node *a0 = vec_get(specs, 0), *an = vec_get(specs, specs->n - 1);
            Diag *d = ck_err("E0119", vec_get(specs, 1), str_from_cstr("An item has one `@spec`: list all its IDs there."));
            if (specs->n == j - i) /* only @spec in between: nothing else is replaced */
                diag_add_fix(d, (int)a0->file, a0->start, an->end, spec_text(specs, 0));
        }
        i = j;
    }
}

void check_items(int file, int mode)
{
    ck_mode = mode;
    FileRec *f = file_get(file);
    ck_scope = f->scope;
    bind_imports(file); /* imported items first (dependencies are already checked) */
    Vec *items = f->ast->a.v;
    for (long i = 0; i < items->n; i++) define_item(vec_get(items, i));
    f->scope = ck_scope;
    for (long i = 0; i < items->n; i++) resolve_item(ck_item_set(vec_get(items, i)));
    check_export_types(items);
    check_tests_unique(items);
    check_annotations(items);
    fx_file_begin();
    for (long i = 0; i < items->n; i++) {
        Node *it = ck_item_set(vec_get(items, i));
        if (it->kind == N_FN)
            check_fn(it);
        else if (it->kind == N_TEST)
            check_test(it);
    }
    ck_item_set(0);
    fx_file_end(items);
    if (ck_mode == 1) check_main(file, find_main(items));
}

int has_unknown_arm(Node *match)
{
    Vec *arms = match->b.v;
    for (long i = 0; i < arms->n; i++) {
        Node *a = arms->data[i].n;
        if (a->kind == N_ARM && a->d.i < 0) return 1;
    }
    return 0;
}

/* A replacement fix with the case of the type whose name matches ignoring case (Ok -> ok) or is
 * within edit distance 2. */
void suggest_case(Diag *d, Type *type, Node *name_node)
{
    long n = type->kind == TY_RESULT ? 2 : count_members(type->decl.n->b.v);
    Str *lower = rt_case(name_node->a.s, 0); /* the written name, lower-cased */
    for (long i = 0; i < n; i++) {
        Str *c = case_name(type, i);
        if (str_eq(c, lower) || name_distance(name_node->a.s, c) <= 2) {
            diag_add_fix(d, (int)name_node->file, name_node->start, name_node->end, c);
            return;
        }
    }
}

/* The convention-conforming name when the node's name breaks the convention, else 0. */
Str *naming_rename(Node *name_node, int upper)
{
    if (!name_node) return 0;
    Str *conv = convert_name(name_node->a.s, upper);
    return str_eq(name_node->a.s, conv) ? 0 : conv;
}

/* At a use of a symbol whose declaration breaks the naming convention: E0207 with the rename. */
void rename_use(Sym *sym, Node *node)
{
    if (!sym->rename) return;
    Diag *d = ck_err("E0207", node,
                     msg("`$1` breaks the naming convention: rename it to `$2` here too.", sym->name, sym->rename, 0));
    diag_add_fix(d, (int)node->file, node->start, node->end, sym->rename);
}
