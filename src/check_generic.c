/* User-defined generics (research §7, contracts/grammar-v2.md; ported from src/check_generic.s,
 * spec 008).
 *
 * Type parameters are symbols (SYM_TPARAM) whose type is a TY_PARAM; generic bodies are checked
 * once with them as opaque types. Type arguments are never written at calls: a call unifies its
 * parameter types with the argument types (and its return type with the expected type) to bind
 * them, E0310 when one stays unbound. The bindings are stored on the call (N_E) for the runtime. */
#include "vo.h"

/* Brings a declaration's type parameters into scope. */
void tparams_push(Vec *tparams, Node *owner)
{
    if (!tparams) return;
    for (long i = 0; i < tparams->n; i++) {
        Node *tp = vec_get(tparams, i); /* N_TPARAM */
        Type *t = ty_make(TY_PARAM, (void *)i, tp->b.p, owner);
        if (!tp->a.n) continue;
        Sym *s = sym_new(tp->a.n->a.s, SYM_TPARAM, owner, t, tp->a.n);
        s->flags = SYF_USED;
        scope_add(s);
    }
}

/* int, float, str, an `ordered` type parameter, or an error type. */
int ty_ordered(Type *t)
{
    switch (t->kind) {
    case TY_INT:
    case TY_FLOAT:
    case TY_STR:
    case TY_ERROR: return 1;
    case TY_PARAM: return t->b.i == TPC_ORDERED;
    default: return 0;
    }
}

/* E0313 when an `ordered` type parameter gets a type without an order. 1 = fine. */
int check_targ(Type *t, Node *tparam, Node *node, Str *owner_name)
{
    if (tparam->b.i != TPC_ORDERED || ty_ordered(t)) return 1;
    ck_err("E0313", node,
           msg("`$1` is not ordered, but `$2` needs `int`, `float` or `str`.", ty_name(t), owner_name, 0));
    return 0;
}

/* E0312; returns the error type. */
Type *ty_argn_error(Node *node, Str *name, long want, long got)
{
    Str *g = int_to_str(got);
    Str *w = int_to_str(want);
    ck_err("E0312", node, msg("`$1` takes $2 type argument(s), got $3.", name, w, g));
    return t_error;
}

/* `Name` or `Name[args]` for a record or variant: its type (an instance when generic), checking
 * the number of arguments (E0312) and `ordered` parameters (E0313). */
Type *resolve_instance(Node *node, Node *decl, int kind, Vec *arg_nodes)
{
    Vec *tparams = ty_tparams_of(decl);
    long np = tparams ? tparams->n : 0;
    long na = arg_nodes ? arg_nodes->n : 0; /* arguments written */
    if (np != na) return ty_argn_error(node, decl->a.n->a.s, np, na);
    if (!np) return ty_make(kind, 0, 0, decl); /* not generic */
    Vec *args = vec_new();
    for (long i = 0; i < na; i++) {
        Node *an = vec_get(arg_nodes, i);
        Type *t = resolve_type(an);
        check_targ(t, vec_get(tparams, i), an, decl->a.n->a.s);
        vec_push(args, t);
    }
    return ty_instance(kind, decl, args);
}

/* A field's type in an instance: the declared type with the instance's arguments substituted. */
Type *field_type_in(Type *record_type, Type *field_type)
{
    if (record_type->kind == TY_ERROR) return record_type; /* failed to resolve: no cascading mismatches */
    if (record_type->b.v && (record_type->kind == TY_RECORD || record_type->kind == TY_VARIANT))
        return ty_subst(field_type, record_type->decl.n, record_type->b.v);
    return field_type;
}

Vec *bindings_vec(Type **bindings, long n)
{
    Vec *v = vec_new();
    for (long i = 0; i < n; i++) vec_push(v, bindings[i]);
    return v;
}

/* After inference: effect parameters left unbound are pure; a type parameter left unbound is
 * E0310 (it becomes the error type). 1 when everything was bound. */
int bind_rest(Node *node, Vec *tparams, Type **bindings, Str *what)
{
    int all = 1;
    for (long i = 0; i < tparams->n; i++) {
        if (bindings[i]) continue;
        Node *tp = vec_get(tparams, i);
        if (tp->b.i == TPC_EFFECTS) { /* an unused effect parameter: pure */
            bindings[i] = ty_make(TY_EFFSET, 0, 0, es_new(0));
            continue;
        }
        all = 0;
        ck_err("E0310", node, msg("Cannot infer `$1` for `$2`: give the variable a type.", tp->a.n->a.s, what, 0));
        bindings[i] = t_error;
    }
    return all;
}

/* A call of a generic function: arguments, inference, constraints, effects; the return type. */
Type *check_generic_call(Node *call, Node *decl, Type *expected)
{
    Vec *tparams = ty_tparams_of(decl);
    Type **b = alloc(tparams->n * 8); /* bindings */
    if (!check_argc(call, decl->b.v->n)) return t_error;
    for (long i = 0; i < decl->b.v->n; i++) { /* each argument: unify its type with the parameter's */
        Type *pt = ((Node *)vec_get(decl->b.v, i))->type;
        Type *at = check_arg(call, i, ty_has_params(pt) ? 0 : pt); /* nothing to infer: check directly */
        if (ty_unify(pt, at, decl, b)) continue;
        Type *known = ty_subst_bindings(pt, decl, b); /* the parameter's type as far as it is known */
        ck_mismatch(call->b.v->data[i].n, known, at);
    }
    if (expected) ty_unify(decl->type, expected, decl, b); /* it can bind what the arguments did not */
    if (!bind_rest(call, tparams, b, call->a.n->a.s))     /* what stayed unbound */
        infer_fix(call, diag_last()); /* E0310: an annotation fix when a later use shows the type */
    for (long i = 0; i < tparams->n; i++) { /* `ordered` parameters */
        Type *t = b[i];
        Node *tp = vec_get(tparams, i);
        if (tp->b.i != TPC_EFFECTS) check_targ(t, tp, call, call->a.n->a.s);
    }
    Vec *targs = bindings_vec(b, tparams->n); /* bindings for the runtime */
    call->e.v = targs;
    for (long i = 0; i < decl->b.v->n; i++) { /* function values must fit the instantiated type */
        Type *want = ty_subst(((Node *)vec_get(decl->b.v, i))->type, decl, targs);
        Node *arg = vec_get(call->b.v, i);
        if (arg->type->kind == TY_FUNC) expect_type(arg, want, arg->type);
    }
    EffSet *declared = decl->a.n->b.fx->declared; /* the declaration with its effect parameters bound */
    if (declared) fx_value_call(es_subst(declared, decl, targs));
    return ty_subst(decl->type, decl, targs);
}

/* ty_subst with a bindings array (as many as owner's type parameters). */
Type *ty_subst_bindings(Type *t, Node *owner, Type **bindings)
{
    Vec *args = bindings_vec(bindings, ty_tparams_of(owner)->n);
    return ty_subst(t, owner, args);
}

/* `Tree.leaf` / `Tree.node(x)` of a generic variant: arguments from the expected type, then from
 * the payload; E0310 for what stays unknown. */
Type *vcase_generic(Node *node, Node *decl, Node *payload_type_node, Type *expected)
{
    Vec *tparams = ty_tparams_of(decl);
    Type **b = alloc(tparams->n * 8); /* bindings */
    if (expected && expected->kind == TY_VARIANT && expected->decl.n == decl && expected->b.v) {
        Vec *args = expected->b.v; /* the expected type, when it is this variant */
        for (long i = 0; i < args->n; i++) b[i] = args->data[i].t;
    }
    if (node->c.n && payload_type_node) { /* the payload */
        Type *at = check_expr(node->c.n, 0);
        if (!ty_unify(payload_type_node->type, at, decl, b)) {
            Type *known = ty_subst_bindings(payload_type_node->type, decl, b);
            ck_mismatch(node->c.n, known, at);
        }
    }
    SB *sb = sb_new(); /* `Tree.case` for messages */
    sb_str(sb, node->a.n->a.s);
    sb_byte(sb, '.');
    sb_str(sb, node->b.n->a.s);
    bind_rest(node, tparams, b, sb_finish(sb));
    return ty_instance(TY_VARIANT, decl, bindings_vec(b, tparams->n));
}

/* For a type parameter where an ordered type is needed: 1 when it is `ordered`; otherwise E0311
 * with a fix adding `: ordered` to its declaration, and 2. 0 for other types. */
int ck_param_order(Type *t, Node *node)
{
    if (t->kind != TY_PARAM) return 0;
    if (t->b.i == TPC_ORDERED) return 1;
    Diag *d = ck_err1("E0311", node,
                      "`$1` has no order: declare it as `$1: ordered` to compare or sort its values.",
                      ty_param_name(t));
    Vec *tparams = ty_tparams_of(t->decl.n); /* the fix: `T` -> `T: ordered` where it is declared */
    if (tparams) {
        Node *nn = ((Node *)vec_get(tparams, t->a.i))->a.n;
        if (nn) diag_add_fix(d, (int)nn->file, nn->end, nn->end, str_from_cstr(": ordered"));
    }
    return 2;
}

/* `let x = f();` where f's type argument cannot be inferred: when the first later statement that
 * calls a generic function with `x` as an argument determines x's type, a fix adds `: Type`. */
void infer_fix(Node *call, Diag *d)
{
    Node *let = ck_cur_stmt;
    if (!let || let->kind != N_LET || let->b.n || let->c.n != call || !let->a.n) return;
    Vec *blk = ck_blk;
    if (!blk) return;
    for (long i = ck_blk_idx; i < blk->n; i++) {
        Node *st = vec_get(blk, i);
        Node *e;
        if (st->kind == N_EXPR_STMT)
            e = st->a.n;
        else if (st->kind == N_LET)
            e = st->c.n;
        else
            continue;
        if (!e || e->kind != N_CALL) continue;
        Type *t = infer_from_use(e, let->a.n->a.s);
        if (!t) continue;
        Str *tn = ty_name(t); /* `let x: Type = ...` */
        SB *sb = sb_new();
        sb_cstr(sb, ": ");
        sb_str(sb, tn);
        Node *nn = let->a.n;
        diag_add_fix(d, (int)nn->file, nn->end, nn->end, sb_finish(sb));
        return;
    }
}

/* The type `name` must have as an argument of this generic call, from its other arguments (checked
 * silently), or 0 when that does not determine it. */
Type *infer_from_use(Node *call, Str *name)
{
    Sym *s = scope_find(call->a.n->a.s);
    if (!s || s->kind != SYM_FN) return 0;
    Node *fn = s->decl; /* the generic function */
    Vec *tparams = ty_tparams_of(fn);
    if (!tparams) return 0;
    Type **b = alloc(tparams->n * 8); /* bindings */
    Vec *args = call->b.v;
    if (args->n != fn->b.v->n) return 0;
    long vi = 0; /* which argument is the variable? */
    for (;; vi++) {
        if (vi >= args->n) return 0;
        Node *a = vec_get(args, vi);
        if (a->kind == N_IDENT && str_eq(a->a.s, name)) break;
    }
    diag_mute = 1; /* the other arguments, silently */
    for (long j = 0; j < args->n; j++) {
        if (j == vi) continue;
        Type *at = check_expr(vec_get(args, j), 0);
        ty_unify(((Node *)vec_get(fn->b.v, j))->type, at, fn, b);
    }
    diag_mute = 0;
    Type *t = ty_subst_bindings(((Node *)vec_get(fn->b.v, vi))->type, fn, b);
    return ty_has_params(t) ? 0 : t;
}
