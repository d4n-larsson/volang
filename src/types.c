/* Function types, generics and the run-time type memo (data-model.md "Types"; contracts/grammar-v2.md;
 * ported from src/types.s, spec 008).
 *
 * TY_FUNC: TY_A = vec of parameter types, TY_B = return type, TY_DECL = effect set. Interned here
 * (structurally: same parameters, return type and effects), so equal types are the same pointer. A
 * function value fits an expected function type when the parameters and return type match and the
 * expected effects cover the value's. */
#include "vo.h"

static Vec *func_tab;  /* interned TY_FUNC types */
Node *ck_exp_tnode;    /* the type node an expected type came from (E0502 fixes), or 0 */

static int same_types(Vec *a, Vec *b)
{
    if (a->n != b->n) return 0;
    for (long i = 0; i < a->n; i++)
        if (a->data[i].p != b->data[i].p) return 0;
    return 1;
}

Type *ty_func(Vec *params, Type *ret, EffSet *effects)
{
    GC_ROOT(&params);
    GC_ROOT(&ret);
    GC_ROOT(&effects);
    if (!func_tab) func_tab = vec_new();
    Type *t;
    for (long i = 0; i < func_tab->n; i++) {
        t = vec_get(func_tab, i);
        if (t->b.t == ret && same_types(t->a.v, params) && es_equal(t->decl.es, effects)) goto done;
    }
    t = alloc(TYPE_SIZE);
    t->kind = TY_FUNC;
    t->a.v = params;
    t->b.t = ret;
    t->decl.es = effects;
    GC_ROOT(&t);
    vec_push(func_tab, t);
    GC_UNROOT(1);
done:
    GC_UNROOT(3);
    return t;
}

/* The type of a named function or a checked anonymous function as a value. */
Type *ty_func_of(Node *fn)
{
    Vec *params = vec_new();
    GC_ROOT(&params);
    Vec *decls = fn->b.v;
    for (long i = 0; i < decls->n; i++) vec_push(params, ((Node *)vec_get(decls, i))->type);
    EffSet *effects = 0; /* declared effects (named), or the inferred ones (anonymous) */
    Fx *fx = item_fx(fn);
    if (fx) effects = fx->declared ? fx->declared : fx->direct;
    if (!effects) effects = es_new(0);
    Type *t = ty_func(params, fn->type, effects);
    GC_UNROOT(1);
    return t;
}

/* 0: fits; 1: different parameters or return type; 2: the value has effects the type does not
 * allow. */
int ty_func_fits(Type *value, Type *expected)
{
    if (value == expected) return 0;
    if (value->kind != TY_FUNC || expected->kind != TY_FUNC) return 1;
    if (value->b.t != expected->b.t) return 1;
    if (!same_types(value->a.v, expected->a.v)) return 1;
    return es_covers(expected->decl.es, value->decl.es) ? 0 : 2;
}

/* `fn(int, str) -> bool uses console` (no `-> void`, no clause when pure). */
void ty_func_name_into(SB *sb, Type *t)
{
    sb_cstr(sb, "fn(");
    Vec *params = t->a.v;
    for (long i = 0; i < params->n; i++) {
        if (i) sb_cstr(sb, ", ");
        ty_name_into(sb, vec_get(params, i));
    }
    sb_byte(sb, ')');
    if (t->b.t->kind != TY_VOID) { /* a pure returned function type before ` uses`: in parentheses */
        int open = t->b.t->kind == TY_FUNC && es_is_empty(t->b.t->decl.es) && !es_is_empty(t->decl.es);
        sb_cstr(sb, open ? " -> (" : " -> ");
        ty_name_into(sb, t->b.t);
        if (open) sb_byte(sb, ')');
    }
    if (!es_is_empty(t->decl.es)) {
        sb_cstr(sb, " uses ");
        es_render(t->decl.es, sb);
    }
}

/* E0502: a function value with effects the expected function type does not allow. The fix adds them
 * to that type when the expected type was written as a type in this program (ck_exp_tnode). */
void ty_effects_error(Node *node, Type *expected, Type *found)
{
    Str *missing = fx_render(es_missing(expected->decl.es, found->decl.es));
    Str *m = msg("This function uses $1, which `$2` does not allow.", missing, ty_name(expected), 0);
    Diag *d = ck_err("E0502", node, m);
    Node *tn = ck_exp_tnode;
    if (!tn || tn->a.i != TY_FUNC) return;
    SB *sb = sb_new();
    if (!tn->d.n) /* an existing clause is rewritten, otherwise one is appended */
        sb_cstr(sb, " uses ");
    else
        sb_cstr(sb, "uses ");
    es_render(es_union(expected->decl.es, found->decl.es), sb);
    Str *text = sb_finish(sb);
    if (tn->d.n)
        diag_add_fix(d, (int)tn->file, tn->d.n->start, tn->d.n->end, text);
    else
        diag_add_fix(d, (int)tn->file, tn->end, tn->end, text);
}

/* ---- generics (research §7) ----
 * TY_PARAM: TY_A = index in the owner's type-parameter list, TY_B = constraint (TPC_*), TY_DECL = the
 * owner (N_FN, N_RECORD or N_VARIANT). Instances of generic records and variants are
 * TY_RECORD/TY_VARIANT with TY_B = vec of type arguments, interned here by declaration and arguments.
 * Effect parameters are TY_PARAMs with TPC_EFFECTS in an effect set's ES_EVARS; their bindings are
 * effect sets. */
static Vec *inst_tab;

/* The type-parameter list (vec of N_TPARAM) of a function, record or variant, or 0. */
Vec *ty_tparams_of(Node *decl)
{
    if (!decl) return 0;
    if (decl->kind == N_RECORD || decl->kind == N_VARIANT) return decl->c.v;
    if (decl->kind != N_FN || !decl->a.n || !decl->a.n->b.fx) return 0;
    return decl->a.n->b.fx->tparams;
}

Str *ty_param_name(Type *param)
{
    Vec *tps = ty_tparams_of(param->decl.n);
    if (!tps) return str_from_cstr("?");
    return ((Node *)vec_get(tps, param->a.i))->a.n->a.s;
}

/* The record or variant type `Name[args]` (interned). */
Type *ty_instance(int kind, Node *decl, Vec *args)
{
    GC_ROOT(&args);
    if (!inst_tab) inst_tab = vec_new();
    Type *t;
    for (long i = 0; i < inst_tab->n; i++) {
        t = inst_tab->data[i].t;
        if (t->decl.n == decl && t->kind == kind && same_types(t->b.v, args)) goto done;
    }
    t = alloc(TYPE_SIZE);
    t->kind = kind;
    t->b.v = args;
    t->decl.n = decl;
    GC_ROOT(&t);
    vec_push(inst_tab, t);
    GC_UNROOT(1);
done:
    GC_UNROOT(1);
    return t;
}

/* `[int, str]`; spec 026: an effect argument last as `uses ...`, left out when it has no effects
 * (and the brackets with it when nothing else is left: `Database`). */
void ty_args_into(SB *sb, Vec *args)
{
    long n = args->n;
    if (n) {
        Type *last = vec_get(args, n - 1);
        if (last->kind == TY_EFFSET && es_is_empty(last->decl.es)) n--;
    }
    if (!n) return;
    sb_cstr(sb, "[");
    for (long i = 0; i < n; i++) {
        if (i) sb_cstr(sb, ", ");
        ty_name_into(sb, vec_get(args, i));
    }
    sb_cstr(sb, "]");
}

int ty_vec_has_params(Vec *types);

/* 1 when the type mentions a type or effect parameter. */
int ty_has_params(Type *t)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_PARAM:
        return 1;
    case TY_LIST:
        return ty_has_params(t->a.t);
    case TY_RESULT: /* spec 032: and the error type */
        return ty_has_params(t->a.t) || (t->b.t && ty_has_params(t->b.t));
    case TY_MAP:
        return ty_has_params(t->a.t) || ty_has_params(t->b.t);
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE:
        return ty_has_params(t->a.t) || (t->b.t && ty_has_params(t->b.t));
    case TY_FUNC: {
        Vec *evars = t->decl.es->evars; /* effect parameters */
        if (evars && evars->n) return 1;
        return ty_has_params(t->b.t) || ty_vec_has_params(t->a.v);
    }
    case TY_RECORD:
    case TY_VARIANT:
        return t->b.v ? ty_vec_has_params(t->b.v) : 0;
    case TY_EFFSET:
        return es_evar_count(t->decl.es) != 0;
    default:
        return 0;
    }
}

int ty_vec_has_params(Vec *types)
{
    for (long i = 0; i < types->n; i++)
        if (ty_has_params(vec_get(types, i))) return 1;
    return 0;
}

Vec *ty_subst_vec(Vec *types, Node *owner, Vec *args);

/* The type with owner's type parameters replaced by args (an argument 0 leaves the parameter). An
 * owner of -1 means any owner (at run time, a frame's own parameters). */
Type *ty_subst(Type *t, Node *owner, Vec *args)
{
    if (!t || !args) return t;
    switch (t->kind) {
    case TY_EFFSET:
        return ty_make(TY_EFFSET, 0, 0, es_subst(t->decl.es, owner, args));
    case TY_PARAM: {
        if (owner != (Node *)-1L && t->decl.n != owner) return t;
        if ((unsigned long)t->a.i >= (unsigned long)args->n) return t;
        Type *arg = args->data[t->a.i].t;
        return arg ? arg : t;
    }
    case TY_LIST:
        return ty_list(ty_subst(t->a.t, owner, args));
    case TY_RESULT: { /* spec 032: and the error type */
        GC_ROOT(&t);
        GC_ROOT(&args);
        Type *ok_t = ty_subst(t->a.t, owner, args);
        GC_ROOT(&ok_t);
        Type *r = ty_result_e(ok_t, t->b.t ? ty_subst(t->b.t, owner, args) : 0);
        GC_UNROOT(3);
        return r;
    }
    case TY_MAP: {
        GC_ROOT(&t);
        GC_ROOT(&args);
        Type *k = ty_subst(t->a.t, owner, args);
        GC_ROOT(&k);
        Type *v = ty_subst(t->b.t, owner, args);
        Type *m = ty_make(TY_MAP, k, v, 0);
        GC_UNROOT(3);
        return m;
    }
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: { /* spec 014 */
        GC_ROOT(&t);
        GC_ROOT(&args);
        Type *k = ty_subst(t->a.t, owner, args);
        GC_ROOT(&k);
        Type *v = t->b.t ? ty_subst(t->b.t, owner, args) : 0;
        Type *m = ty_make(t->kind, k, v, 0);
        GC_UNROOT(3);
        return m;
    }
    case TY_FUNC: {
        GC_ROOT(&t);
        GC_ROOT(&args);
        Vec *params = ty_subst_vec(t->a.v, owner, args);
        GC_ROOT(&params);
        Type *ret = ty_subst(t->b.t, owner, args);
        GC_ROOT(&ret);
        EffSet *eff = es_subst(t->decl.es, owner, args);
        Type *f = ty_func(params, ret, eff);
        GC_UNROOT(4);
        return f;
    }
    case TY_RECORD:
    case TY_VARIANT: {
        if (!t->b.v) return t;
        GC_ROOT(&t);
        Vec *sub = ty_subst_vec(t->b.v, owner, args);
        Type *i = ty_instance((int)t->kind, t->decl.n, sub);
        GC_UNROOT(1);
        return i;
    }
    default:
        return t;
    }
}

Vec *ty_subst_vec(Vec *types, Node *owner, Vec *args)
{
    GC_ROOT(&types);
    GC_ROOT(&args);
    Vec *out = vec_new();
    GC_ROOT(&out);
    for (long i = 0; i < types->n; i++) vec_push(out, ty_subst(vec_get(types, i), owner, args));
    GC_UNROOT(3);
    return out;
}

/* Matches type against pattern, binding owner's parameters (bindings[index]; effect parameters bind
 * to effect sets). 1 when they match; errors match anything. */
int ty_unify(Type *pattern, Type *t, Node *owner, Type **bindings)
{
    if (!pattern || !t || t->kind == TY_ERROR) return 1;
    if (pattern->kind == TY_PARAM && pattern->decl.n == owner) { /* owner's parameter: bind or compare */
        Type **slot = &bindings[pattern->a.i];
        if (!*slot) {
            *slot = t;
            return 1;
        }
        return *slot == t;
    }
    if (pattern == t && !ty_has_params(pattern)) return 1; /* with parameters it still binds them */
    if (pattern->kind != t->kind) return 0;
    switch (pattern->kind) {
    case TY_LIST:
        return ty_unify(pattern->a.t, t->a.t, owner, bindings);
    case TY_RESULT: /* spec 032: the error types too (a `str` error when not written) */
        return ty_unify(pattern->a.t, t->a.t, owner, bindings) && ty_unify(err_type(pattern), err_type(t), owner, bindings);
    case TY_MAP:
        return ty_unify(pattern->a.t, t->a.t, owner, bindings) && ty_unify(pattern->b.t, t->b.t, owner, bindings);
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE:
        return ty_unify(pattern->a.t, t->a.t, owner, bindings) &&
               (!pattern->b.t || ty_unify(pattern->b.t, t->b.t, owner, bindings));
    case TY_FUNC: {
        if (pattern->a.v->n != t->a.v->n) return 0; /* same number of parameters */
        for (long i = 0; i < pattern->a.v->n; i++)
            if (!ty_unify(vec_get(pattern->a.v, i), vec_get(t->a.v, i), owner, bindings)) return 0;
        if (!ty_unify(pattern->b.t, t->b.t, owner, bindings)) return 0;
        Vec *evars = pattern->decl.es->evars; /* an effect parameter binds to the value's effects */
        if (!evars || !evars->n) return 1;
        Type *ev = evars->data[0].t;
        if (ev->decl.n != owner) return 1;
        Type **slot = &bindings[ev->a.i];
        if (*slot) return 1;
        *slot = ty_make(TY_EFFSET, 0, 0, t->decl.es);
        return 1;
    }
    case TY_RECORD:
    case TY_VARIANT:
        if (pattern->decl.n != t->decl.n || !pattern->b.v || !t->b.v) return 0;
        for (long i = 0; i < pattern->b.v->n; i++)
            if (!ty_unify(vec_get(pattern->b.v, i), vec_get(t->b.v, i), owner, bindings)) return 0;
        return 1;
    case TY_EFFSET: { /* spec 026: a type's effect argument; owner's lone effect parameter binds to the value's */
        EffSet *p = pattern->decl.es;
        if (es_evar_count(p) == 1 && !p->mask && !p->any && !(p->scopes && p->scopes->n)) {
            Type *ev = vec_get(p->evars, 0);
            if (ev->decl.n == owner) {
                Type **slot = &bindings[ev->a.i];
                if (!*slot) *slot = t;
                return *slot == t;
            }
        }
        return 0; /* (equal interned sets matched above) */
    }
    default:
        return 0;
    }
}

/* ---- run-time memo (spec 003 research §6, data-model.md "Run-time memo") ----
 * Generic code resolves its types at run time. The results are permanent (interned types and
 * type-argument vecs in the permanent arena), so they are memoized by the pointers they depend on: a
 * hot generic loop pays one table lookup instead of a substitution walk and an allocation. */
enum { MEMO_HASP = 1, MEMO_HASPV, MEMO_TARGS, MEMO_CONCRETE, MEMO_ESUBST, MEMO_EGRANT };

typedef struct MemoEntry {
    unsigned long a, b, kind, value;
} MemoEntry;

static MemoEntry *memo_tab; /* open addressing, cap a power of two */
static unsigned long memo_cap;
static long memo_n;

/* The first probe index for a key (memo_cap must be non-zero). */
static unsigned long memo_slot(unsigned long kind, unsigned long a, unsigned long b)
{
    unsigned long h = (a * 0x9E3779B97F4A7C15UL) ^ (b * 0xC2B2AE3D27D4EB4FUL);
    h += kind;
    h ^= h >> 29;
    return h & (memo_cap - 1);
}

/* The stored value, or 0. */
long memo_get(long kind, long a, long b)
{
    if (!memo_cap) return 0;
    unsigned long i = memo_slot((unsigned long)kind, (unsigned long)a, (unsigned long)b);
    for (;; i = (i + 1) & (memo_cap - 1)) {
        MemoEntry *e = &memo_tab[i];
        if (!e->kind) return 0;
        if (e->kind == (unsigned long)kind && e->a == (unsigned long)a && e->b == (unsigned long)b)
            return (long)e->value;
    }
}

/* Stores into the current table, which has room (no growth). */
static void memo_insert(unsigned long kind, unsigned long a, unsigned long b, unsigned long value)
{
    unsigned long i = memo_slot(kind, a, b);
    while (memo_tab[i].kind) i = (i + 1) & (memo_cap - 1);
    memo_tab[i].a = a;
    memo_tab[i].b = b;
    memo_tab[i].kind = kind;
    memo_tab[i].value = value;
    memo_n++;
}

/* Adds an entry; the table doubles (in the permanent arena) before it is half full. */
void memo_put(long kind, long a, long b, long value)
{
    if ((unsigned long)(memo_n + 1) * 2 > memo_cap) { /* grow: rehash into a new table */
        MemoEntry *old = memo_tab;
        unsigned long old_cap = memo_cap;
        unsigned long cap = memo_cap * 2;
        if (cap < 1024) cap = 1024;
        memo_cap = cap;
        memo_n = 0;
        memo_tab = perm_alloc((long)cap * 32); /* zeroed: the arena is never reused */
        while (old_cap) {
            old_cap--;
            if (old[old_cap].kind) memo_insert(old[old_cap].kind, old[old_cap].a, old[old_cap].b, old[old_cap].value);
        }
    }
    memo_insert((unsigned long)kind, (unsigned long)a, (unsigned long)b, (unsigned long)value);
}

static int ty_has_params_memo(Type *t)
{
    long v = memo_get(MEMO_HASP, (long)t, 0);
    if (!v) {
        v = ty_has_params(t) + 1;
        memo_put(MEMO_HASP, (long)t, 0, v);
    }
    return (int)(v - 1);
}

/* At run time: the type with the running frame's type arguments substituted (memoized). */
Type *rt_concrete(Type *t)
{
    if (!t || (unsigned long)t->kind <= TY_VOID) return t; /* int, float, bool, str, void */
    Frame *frame = (Frame *)cur_frame;
    if (t->kind == TY_PARAM) { /* a bare type parameter: the frame's argument, if it has one */
        if (!frame || !frame->targs) return t;
        if ((unsigned long)t->a.i >= (unsigned long)frame->targs->n) return t;
        Type *arg = frame->targs->data[t->a.i].t;
        return arg ? arg : t;
    }
    if (!ty_has_params_memo(t) || !frame || !frame->targs) return t;
    Vec *targs = frame->targs;
    Type *r = (Type *)memo_get(MEMO_CONCRETE, (long)t, (long)targs);
    if (r) return r;
    perm_begin(); /* interned types are never collected (spec 003) */
    r = ty_subst(t, (Node *)-1L, targs);
    perm_end();
    memo_put(MEMO_CONCRETE, (long)t, (long)targs, (long)r);
    return r;
}

/* A generic call's type arguments, with the caller's own type parameters resolved (memoized, so the
 * same call from the same instantiation always yields the same vec). */
Vec *rt_targs(Vec *bindings, Frame *caller)
{
    long has = memo_get(MEMO_HASPV, (long)bindings, 0);
    if (!has) {
        has = ty_vec_has_params(bindings) + 1;
        memo_put(MEMO_HASPV, (long)bindings, 0, has);
    }
    if (has == 1 || !caller || !caller->targs) return bindings; /* no parameters in the bindings */
    Vec *targs = caller->targs;
    Vec *r = (Vec *)memo_get(MEMO_TARGS, (long)bindings, (long)targs);
    if (r) return r;
    perm_begin(); /* type-argument vecs outlive the call (spec 003) */
    r = ty_subst_vec(bindings, (Node *)-1L, targs);
    perm_end();
    memo_put(MEMO_TARGS, (long)bindings, (long)targs, (long)r);
    return r;
}

/* es_subst for a generic call's declared effects, memoized by (set, args). */
EffSet *rt_es_subst(EffSet *set, Node *owner, Vec *args)
{
    if (!es_evar_count(set)) return set;
    EffSet *r = (EffSet *)memo_get(MEMO_ESUBST, (long)set, (long)args);
    if (r) return r;
    r = es_subst(set, owner, args);
    memo_put(MEMO_ESUBST, (long)set, (long)args, (long)r);
    return r;
}

/* Spec 013 (C3): the grant of a frame whose declaration has effect parameters: its own declared
 * effects, with every effect parameter standing for the caller's grant. The parameter's binding is
 * a requirement set ("any scope" bits) and is never used as a grant. */
EffSet *rt_es_grant(EffSet *declared, EffSet *caller)
{
    if (!declared || !es_evar_count(declared)) return declared;
    EffSet *r = (EffSet *)memo_get(MEMO_EGRANT, (long)declared, (long)caller);
    if (r) return r;
    perm_begin(); /* memoized: never collected */
    EffSet *own = es_new((long)declared->mask);
    own->scopes = declared->scopes;
    r = caller ? es_union(own, caller) : own;
    perm_end();
    memo_put(MEMO_EGRANT, (long)declared, (long)caller, (long)r);
    return r;
}
