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
        Sym *set = tp->b.i == TPC_EFFECTS ? scope_find(tp->a.n->a.s) : 0;
        if (set && set->kind == SYM_EFFECTS && !tp->e.i) { /* spec 030: one meaning per name (no shadowing) */
            tp->e.i = 1; /* pushed again for the body: reported once */
            Diag *d = ck_err1("E0203", tp->a.n, "`$1` is already an effect set: give the effect parameter another name.",
                              tp->a.n->a.s);
            Node *pn = set->node;
            diag_add_related(d, (int)pn->file, pn->start, pn->end, msg("`$1` is declared here.", pn->a.s, 0, 0));
        }
        Sym *s = sym_new(tp->a.n->a.s, SYM_TPARAM, owner, t, tp->a.n);
        s->flags = SYF_USED;
        scope_add(s);
    }
}

static int ty_ordered_d(Type *t, long depth);

/* Spec 014 (FR-012): the first field of record type t that has no order, or 0 (t is ordered field by
 * field). depth bounds records that contain records. */
static Node *unordered_field(Type *t, long depth)
{
    Vec *fields = t->decl.n->b.v;
    for (long i = 0;; i++) {
        Node *m = member_at(fields, i);
        if (!m) return 0;
        if (m->kind == N_COMMENT) continue;
        if (!ty_ordered_d(field_type_in(t, m->b.n->type), depth + 1)) return m;
    }
}

static int ty_ordered_d(Type *t, long depth)
{
    switch (t->kind) {
    case TY_INT:
    case TY_FLOAT:
    case TY_STR:
    case TY_BYTES: /* spec 021: byte by byte */
    case TY_ERROR: return 1;
    case TY_BOOL: return 1; /* spec 056: false < true */
    case TY_PARAM: return t->b.i == TPC_ORDERED || t->b.i == TPC_KEY; /* spec 056: every key type is ordered */
    case TY_RECORD: return depth < 32 && !unordered_field(t, depth); /* spec 014: records of ordered fields */
    case TY_VARIANT: { /* spec 056: by case (declaration order), then the payload */
        if (depth >= 32) return 0;
        Node *decl = t->decl.n;
        Vec *tps = ty_tparams_of(decl);
        for (long i = 0; i < decl->b.v->n; i++) {
            Node *m = vec_get(decl->b.v, i);
            if (m->kind == N_COMMENT || !m->b.n || !m->b.n->type) continue; /* (a case without a value) */
            Type *mt = tps && tps->n && t->b.v ? ty_subst(m->b.n->type, decl, t->b.v) : m->b.n->type;
            if (!ty_ordered_d(mt, depth + 1)) return 0;
        }
        return 1;
    }
    default: return 0;
    }
}

/* int, float, str, bytes, bool, a record of ordered fields, a variant of ordered payloads, an `ordered` or
 * `key` type parameter, or an error type. */
int ty_ordered(Type *t)
{
    return ty_ordered_d(t, 0);
}

/* E0313 at node for a type with no order: a record names its first field without one. */
void order_error(Type *t, Node *node, char *needed_by)
{
    Node *f = t->kind == TY_RECORD ? unordered_field(t, 0) : 0;
    if (f) {
        SB *sb = sb_new();
        sb_byte(sb, '`');
        sb_str(sb, f->a.n->a.s);
        sb_cstr(sb, "` (`");
        ty_name_into(sb, field_type_in(t, f->b.n->type));
        sb_cstr(sb, "`)");
        ck_err("E0313", node, msg("`$1` is not ordered: its field $2 has no order.", ty_name(t), sb_finish(sb), 0));
        return;
    }
    if (t->kind == TY_VARIANT) { /* spec 056: the first case whose payload has no order */
        Node *decl = t->decl.n;
        Vec *tps = ty_tparams_of(decl);
        for (long i = 0; i < decl->b.v->n; i++) {
            Node *m = vec_get(decl->b.v, i);
            if (m->kind == N_COMMENT || !m->b.n || !m->b.n->type) continue;
            Type *mt = tps && tps->n && t->b.v ? ty_subst(m->b.n->type, decl, t->b.v) : m->b.n->type;
            if (ty_ordered(mt)) continue;
            ck_err("E0313", node, msg("`$1` is not ordered: its case `$2` holds `$3`, which has no order.", ty_name(t), m->a.n->a.s, ty_name(mt)));
            return;
        }
    }
    ck_err("E0313", node, msg(needed_by ? needed_by : "`$1` has no order.", ty_name(t), 0, 0));
}

/* E0313 when an `ordered` type parameter gets a type without an order. 1 = fine. */
/* Spec 029: 0 when t converts to and from JSON (research R2); otherwise why the first part that does not
 * cannot, with its path from the top (`items[].on_save` is a function). */
static Str *json_part(Str *path, char *what, Str *a)
{
    Str *w = msg(what, a, 0, 0);
    return path->len ? msg("`$1` $2", path, w, 0) : msg("it $1", w, 0, 0);
}

static Vec *json_seen; /* the records and variants already looked at in this check (recursive types) */

static Str *json_why(Type *t, Str *path, long depth)
{
    if (!t || depth > 64) return 0;
    switch (t->kind) {
    case TY_INT:
    case TY_FLOAT:
    case TY_BOOL:
    case TY_STR:
    case TY_BYTES:
    case TY_ERROR: return 0;
    case TY_PARAM:
        if (t->b.i == TPC_JSON) return 0;
        return json_part(path, "is the type parameter `$1`, which needs `: json`", ty_param_name(t));
    case TY_LIST: return json_why(t->a.t, msg("$1[]", path, 0, 0), depth + 1);
    case TY_MAP:
        if (t->a.t->kind != TY_STR) return json_part(path, "is a map with `$1` keys (JSON keys are text)", ty_name(t->a.t));
        return json_why(t->b.t, msg("$1[]", path, 0, 0), depth + 1);
    case TY_RECORD:
    case TY_VARIANT: {
        for (long i = 0; i < json_seen->n; i++)
            if (vec_get(json_seen, i) == t) return 0; /* (each instance once: recursive types end) */
        vec_push(json_seen, t);
        Node *decl = t->decl.n;
        Vec *tps = ty_tparams_of(decl);
        for (long i = 0; tps && i < tps->n; i++)
            if (((Node *)vec_get(tps, i))->b.i == TPC_EFFECTS) return json_part(path, "is `$1`, which has effects", ty_name(t));
        Vec *ms = decl->b.v;
        for (long i = 0; i < ms->n; i++) {
            Node *m = vec_get(ms, i);
            if (m->kind == N_COMMENT || !m->b.n || !m->b.n->type) continue;
            Type *mt = tps && tps->n && t->b.v ? ty_subst(m->b.n->type, decl, t->b.v) : m->b.n->type;
            Str *p = path->len ? msg("$1.$2", path, m->a.n->a.s, 0) : m->a.n->a.s;
            Str *why = json_why(mt, p, depth + 1);
            if (why) return why;
        }
        return 0;
    }
    case TY_FUNC: return json_part(path, "is a function", 0);
    case TY_RESULT: return json_part(path, "is a result", 0);
    default: return json_part(path, "is `$1`, which has no JSON form", ty_name(t));
    }
}

/* Spec 033: 0 when t can be a map key (research R1), otherwise why the first part that cannot, with
 * its path from the top (`items` is a list). */
static Vec *key_seen; /* the records and variants already looked at in this check (recursive types) */

static Str *key_why_d(Type *t, Str *path, long depth)
{
    if (!t || depth > 64) return 0;
    switch (t->kind) {
    case TY_INT:
    case TY_BOOL:
    case TY_STR:
    case TY_BYTES:
    case TY_ERROR: return 0;
    case TY_FLOAT:
        if (!path->len) return str_from_cstr("key by `str(x)` or a scaled `int`");
        return json_part(path, "is `float`", 0);
    case TY_PARAM:
        if (t->b.i == TPC_KEY) return 0;
        if (!path->len)
            return t->b.i == TPC_ORDERED ? str_from_cstr("it is declared `ordered`, not `key`")
                 : t->b.i == TPC_JSON    ? str_from_cstr("it is declared `json`, not `key`")
                                         : msg("declare it as `$1: key`", ty_param_name(t), 0, 0);
        return json_part(path, "is the type parameter `$1`, which needs `: key`", ty_param_name(t));
    case TY_LIST: return json_part(path, "is a list, which can change", 0);
    case TY_MAP: return json_part(path, "is a map, which can change", 0);
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return json_part(path, "is `$1`, which can change", ty_name(t));
    case TY_FUNC: return json_part(path, "is a function", 0);
    case TY_RESULT: return json_part(path, "is a result", 0);
    case TY_RECORD:
    case TY_VARIANT: {
        for (long i = 0; i < key_seen->n; i++)
            if (vec_get(key_seen, i) == t) return 0; /* (each instance once: recursive types end) */
        vec_push(key_seen, t);
        Node *decl = t->decl.n;
        Vec *tps = ty_tparams_of(decl);
        Vec *ms = decl->b.v;
        for (long i = 0; i < ms->n; i++) {
            Node *m = vec_get(ms, i);
            if (m->kind == N_COMMENT || !m->b.n || !m->b.n->type) continue; /* (a case without a value) */
            Type *mt = tps && tps->n && t->b.v ? ty_subst(m->b.n->type, decl, t->b.v) : m->b.n->type;
            Str *p = path->len ? msg("$1.$2", path, m->a.n->a.s, 0) : m->a.n->a.s;
            Str *why = key_why_d(mt, p, depth + 1);
            if (why) return why;
        }
        return 0;
    }
    default: return json_part(path, "is `$1`, which cannot be a key", ty_name(t));
    }
}

Str *key_why(Type *t)
{
    key_seen = vec_new();
    return key_why_d(t, str_from_cstr(""), 0);
}

/* While the items' types are being resolved, a record's fields may not be yet: key checks wait in this
 * list (type, node pairs) until every item is resolved (key_checks_end). */
static Vec *key_pending;

/* E0309 at node when t cannot be a map key; a bare type parameter gets the fix `: key`. */
static void key_report(Type *t, Node *node)
{
    Str *why = key_why(t);
    if (!why) return;
    Diag *d = ck_err("E0309", node, msg("`$1` cannot be a map key: $2.", ty_name(t), why, 0));
    if (t->kind == TY_PARAM && !t->b.i) { /* a bare `K`: the fix declares it `K: key` */
        Vec *tps = ty_tparams_of(t->decl.n);
        Node *nn = tps ? ((Node *)vec_get(tps, t->a.i))->a.n : 0;
        if (nn) diag_add_fix(d, (int)nn->file, nn->end, nn->end, str_from_cstr(": key"));
    }
}

void key_check(Type *t, Node *node)
{
    if (!key_pending) {
        key_report(t, node);
        return;
    }
    vec_push(key_pending, t);
    vec_push(key_pending, node);
}

Vec *key_checks_begin(void)
{
    Vec *saved = key_pending;
    key_pending = vec_new();
    return saved;
}

void key_checks_end(Vec *saved)
{
    Vec *v = key_pending;
    key_pending = 0;
    for (long i = 0; i + 1 < v->n; i += 2) key_report(vec_get(v, i), vec_get(v, i + 1));
    key_pending = saved;
}

int check_targ(Type *t, Node *tparam, Node *node, Str *owner_name)
{
    if (tparam->b.i == TPC_KEY) { /* spec 033: E0309 */
        (void)owner_name;
        key_check(t, node);
        return 1;
    }
    if (tparam->b.i == TPC_JSON) { /* spec 029: E0328 */
        json_seen = vec_new();
        Str *why = json_why(t, str_from_cstr(""), 0);
        if (!why) return 1;
        Diag *d = ck_err("E0328", node, msg("`$1` cannot be converted to JSON: $2.", ty_name(t), why, 0));
        if (t->kind == TY_PARAM && !t->b.i) { /* a bare `T`: the fix declares it `T: json` */
            Vec *tps = ty_tparams_of(t->decl.n);
            Node *nn = tps ? ((Node *)vec_get(tps, t->a.i))->a.n : 0;
            if (nn) diag_add_fix(d, (int)nn->file, nn->end, nn->end, str_from_cstr(": json"));
        }
        return 0;
    }
    if (tparam->b.i != TPC_ORDERED || ty_ordered(t)) return 1;
    if (t->kind == TY_RECORD) {
        order_error(t, node, 0);
        return 0;
    }
    ck_err("E0313", node,
           msg("`$1` is not ordered, but `$2` needs `int`, `float` or `str`.", ty_name(t), owner_name, 0));
    return 0;
}

/* E0312; returns the error type. */
Type *ty_argn_error(Node *node, Str *name, long want, long got)
{
    Str *g = int_to_str(got);
    Str *w = int_to_str(want);
    ck_err("E0312", node, msg("`$1` takes $2 type argument(s), found $3.", name, w, g));
    return t_error;
}

/* `Name` or `Name[args]` for a record or variant: its type (an instance when generic), checking
 * the number of arguments (E0312) and `ordered` parameters (E0313). */
static int effects_named(Node *uses);

Type *resolve_instance(Node *node, Node *decl, int kind, Vec *arg_nodes)
{
    Vec *tparams = ty_tparams_of(decl);
    long np = tparams ? tparams->n : 0;
    long na = arg_nodes ? arg_nodes->n : 0; /* arguments written */
    /* spec 026: a last effect parameter takes the `uses ...` argument; written without it, no effects */
    int eff_last = np && ((Node *)vec_get(tparams, np - 1))->b.i == TPC_EFFECTS;
    Node *uses = na ? vec_get(arg_nodes, na - 1) : 0;
    if (uses && uses->kind != N_USES) uses = 0;
    for (long i = 0; i < na; i++) { /* E0327: effects in the wrong form or place */
        Node *an = vec_get(arg_nodes, i);
        if (an->kind != N_USES) continue;
        if (!eff_last) return effect_arg_error(an, decl, 1);
        if (i != na - 1 || !effects_named(an)) return effect_arg_error(an, decl, 2);
        if (an->b.i) return effect_arg_error(an, decl, 3);
    }
    /* the effect argument: `uses ...`, an effect parameter's name (`Conn[E]`, na == np), or left out */
    long given = na + (eff_last && !uses && na == np - 1 ? 1 : 0);
    if (np != given) return ty_argn_error(node, decl->a.n->a.s, np - eff_last, na - (uses != 0));
    if (!np) return ty_make(kind, 0, 0, decl); /* not generic */
    Vec *args = vec_new();
    for (long i = 0; i < np; i++) {
        Node *an = i < na ? vec_get(arg_nodes, i) : 0;
        Type *t;
        if (!an) { /* the effect argument left out: no effects */
            t = ty_make(TY_EFFSET, 0, 0, es_new(0));
        } else if (an->kind == N_USES) {
            /* unscoped effects in a type mean "in some scope", as a library function's own `uses`: the
               value's real target (a database's path) is checked at run time (spec 026, research R14) */
            EffSet *es = fx_build_set(an, 0);
            for (long k = 0; k < an->a.v->n; k++) /* written scopes; a set's are dropped (spec 030) */
                if (((Node *)vec_get(an->a.v, k))->b.v) return scoped_effect_arg_error(an, es);
            for (long k = 0; es->scopes && k < es->scopes->n; k++) es->any |= (unsigned long)((EffScope *)vec_get(es->scopes, k))->bit;
            es->scopes = 0;
            es->any |= es->mask;
            es->mask = 0;
            t = ty_make(TY_EFFSET, 0, 0, es);
        } else {
            Node *tp = vec_get(tparams, i);
            Sym *set = an->kind == N_TYPE_NODE && an->a.i == TY_NAMED && an->b.n ? scope_find(an->b.n->a.s) : 0;
            if (tp->b.i == TPC_EFFECTS && set && set->kind == SYM_EFFECTS) { /* spec 056: `Store[Storage]` is */
                EffSet *es = es_union(es_new(0), fx_set_of(set->decl, an));  /* `Store[uses Storage]` (vo fmt) */
                q_note_sym(set, an->b.n, RR_USE);
                for (long k = 0; es->scopes && k < es->scopes->n; k++) es->any |= (unsigned long)((EffScope *)vec_get(es->scopes, k))->bit;
                es->scopes = 0;
                es->any |= es->mask;
                es->mask = 0;
                vec_push(args, ty_make(TY_EFFSET, 0, 0, es));
                continue;
            }
            t = resolve_type(an);
            int eff_param = t->kind == TY_PARAM && t->b.i == TPC_EFFECTS;
            if (tp->b.i == TPC_EFFECTS && !eff_param && !ty_bad(t)) return effect_arg_error(an, decl, 3);
            if (tp->b.i != TPC_EFFECTS && eff_param) return effect_arg_error(an, decl, 1);
            check_targ(t, tp, an, decl->a.n->a.s);
            if (eff_param) { /* `Conn[E]` is `Conn[uses E]`: one form, so instances unify and compare */
                EffSet *es = es_new(0);
                es_add_evar(es, t);
                t = ty_make(TY_EFFSET, 0, 0, es);
            }
        }
        vec_push(args, t);
    }
    return ty_instance(kind, decl, args);
}

/* Spec 026, E0327: scopes in an effect argument. A type names effects; the scopes are the grant's, checked
 * where the value is used. The fix drops the scopes. Returns the error type. */
Type *scoped_effect_arg_error(Node *an, EffSet *es)
{
    EffSet *plain = es_new((long)es->mask);
    plain->evars = es->evars;
    for (long i = 0; i < es->scopes->n; i++) plain->mask |= (unsigned long)((EffScope *)vec_get(es->scopes, i))->bit;
    Str *fixed = ty_name(ty_make(TY_EFFSET, 0, 0, plain));
    Diag *d = ck_err("E0327", an,
                     msg("A type names effects without scopes: `$1`. The scopes are the grant's, checked where the value "
                         "is used.",
                         fixed, 0, 0));
    diag_add_fix(d, (int)an->file, an->start, an->end, fixed);
    return t_error;
}

/* Spec 052: whether every capitalized name in an effect argument is an effect parameter or set in scope.
 * The parser reads a type name after written effects as an effect (`Job[uses clock, D]`), so a type
 * written after the effects (`Two[uses console, Note]`) is caught here as effects not written last. */
static int effects_named(Node *uses)
{
    for (long i = 0; i < uses->a.v->n; i++) {
        Node *e = vec_get(uses->a.v, i);
        if (!e->c.n) continue;
        int found = 0;
        for (Sym *sy = ck_scope; sy && !found; sy = sy->next)
            found = str_eq(sy->name, e->d.s) && (sy->kind == SYM_EFFECTS || (sy->kind == SYM_TPARAM && sy->type->b.i == TPC_EFFECTS));
        if (!found && i) return 0;
    }
    return 1;
}

/* Spec 026, E0327: an effect argument where the type has no effect parameter (why 1), not last (2), or
 * written without `uses` (3, with the fix inserting it). Returns the error type. */
Type *effect_arg_error(Node *an, Node *decl, int why)
{
    if (why == 1) {
        ck_err("E0327", an, msg("`$1` has no effect parameter.", decl->a.n->a.s, 0, 0));
    } else if (why == 2) {
        ck_err("E0327", an, str_from_cstr("Effects in a type are written last, after `uses`."));
    } else {
        SB *sb = sb_new();
        sb_cstr(sb, "uses ");
        sb_str(sb, src_text(an));
        Str *fixed = sb_finish(sb);
        Diag *d = ck_err("E0327", an, msg("Effects in a type are written last, after `uses`: `$1`.", fixed, 0, 0));
        diag_add_fix(d, (int)an->file, an->start, an->start, str_from_cstr("uses "));
    }
    return t_error;
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

/* 1 when t contains a function type anywhere (spec 013, C2: such arguments are checked again after
 * inference, since unification does not compare effects). */
int ty_has_func(Type *t)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_FUNC: return 1;
    case TY_LIST:
    case TY_RESULT: return ty_has_func(t->a.t) || (t->b.t && ty_has_func(t->b.t));
    case TY_MAP: return ty_has_func(t->a.t) || ty_has_func(t->b.t);
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return ty_has_func(t->a.t) || (t->b.t && ty_has_func(t->b.t));
    case TY_RECORD:
    case TY_VARIANT:
        for (long i = 0; t->b.v && i < t->b.v->n; i++)
            if (ty_has_func(vec_get(t->b.v, i))) return 1;
        return 0;
    default: return 0;
    }
}

static Vec *func_seen; /* the records and variants already looked at (recursive types) */

static int holds_func_d(Type *t, long depth)
{
    if (!t || depth > 64) return 0;
    switch (t->kind) {
    case TY_FUNC: return 1;
    case TY_LIST:
    case TY_RESULT:
    case TY_MAP:
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return holds_func_d(t->a.t, depth + 1) || (t->b.t && holds_func_d(t->b.t, depth + 1));
    case TY_RECORD:
    case TY_VARIANT: {
        for (long i = 0; i < func_seen->n; i++)
            if (vec_get(func_seen, i) == t) return 0;
        vec_push(func_seen, t);
        Node *decl = t->decl.n;
        Vec *tps = ty_tparams_of(decl);
        for (long i = 0; i < decl->b.v->n; i++) {
            Node *m = vec_get(decl->b.v, i);
            if (m->kind == N_COMMENT || !m->b.n || !m->b.n->type) continue;
            Type *mt = tps && tps->n && t->b.v ? ty_subst(m->b.n->type, decl, t->b.v) : m->b.n->type;
            if (holds_func_d(mt, depth + 1)) return 1;
        }
        return 0;
    }
    default: return 0;
    }
}

/* Spec 055 follow-up: 1 when a value of type t holds a function anywhere (a field, an element, a case's
 * value): such values cannot be compared, as functions cannot (E0308). */
int ty_holds_func(Type *t)
{
    func_seen = vec_new();
    return holds_func_d(t, 0);
}

/* Spec 014 (research R8): 1 when t mentions a type parameter of owner (one not bound yet). */
int ty_mentions_owner(Type *t, Node *owner)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_PARAM: return t->decl.n == owner;
    case TY_LIST:
    case TY_RESULT: return ty_mentions_owner(t->a.t, owner) || (t->b.t && ty_mentions_owner(t->b.t, owner));
    case TY_MAP:
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return ty_mentions_owner(t->a.t, owner) || (t->b.t && ty_mentions_owner(t->b.t, owner));
    case TY_FUNC:
        if (ty_mentions_owner(t->b.t, owner)) return 1;
        for (long i = 0; i < t->a.v->n; i++)
            if (ty_mentions_owner(vec_get(t->a.v, i), owner)) return 1;
        return 0;
    case TY_RECORD:
    case TY_VARIANT:
        for (long i = 0; t->b.v && i < t->b.v->n; i++)
            if (ty_mentions_owner(vec_get(t->b.v, i), owner)) return 1;
        return 0;
    case TY_EFFSET: /* spec 026: a type's effect argument */
        for (long i = 0; i < es_evar_count(t->decl.es); i++)
            if (((Type *)vec_get(t->decl.es->evars, i))->decl.n == owner) return 1;
        return 0;
    default: return 0;
    }
}

/* Spec 014 (R8): an expression whose type comes from where it is used: a generic case without a value,
 * `new` without type arguments, a call without arguments (a constructor such as `ordered_map()`). */
int needs_context(Node *v)
{
    if (v->kind == N_IDENT) { /* spec 028: a generic function as a value takes its bindings from the parameter */
        Sym *s = scope_find(v->a.s);
        Vec *tp = s && s->kind == SYM_FN ? ty_tparams_of(s->decl) : 0;
        return tp && tp->n;
    }
    return (v->kind == N_VCASE && !v->c.n) || (v->kind == N_NEW_RECORD && (!v->d.v || !v->d.v->n)) ||
           (v->kind == N_CALL && v->b.v && !v->b.v->n);
}

Node *ck_infer_owner; /* spec 029: the generic call whose deferred argument is being checked, or 0 */

/* Spec 059: the first type parameter of owner that t mentions (unbound), or 0. */
static Type *open_param_in(Type *t, Node *owner)
{
    if (!t || !ty_mentions_owner(t, owner)) return 0;
    if (t->kind == TY_PARAM || t->kind == TY_EFFSET) return t; /* (an effect argument: `uses E`) */
    if (t->kind == TY_FUNC) {
        for (long i = 0; i < t->a.v->n; i++) {
            Type *p = open_param_in(vec_get(t->a.v, i), owner);
            if (p) return p;
        }
        return open_param_in(t->b.t, owner);
    }
    if (t->kind == TY_RECORD || t->kind == TY_VARIANT) {
        for (long i = 0; t->b.v && i < t->b.v->n; i++) {
            Type *p = open_param_in(vec_get(t->b.v, i), owner);
            if (p) return p;
        }
        return 0;
    }
    Type *p = open_param_in(t->a.t, owner); /* list, map, result and the collections: a, then b */
    return p ? p : open_param_in(t->b.t, owner);
}

/* Spec 059 (research R3): the expected type of the anonymous function `fnode` whose parameter type is known
 * so far: its parameters and its return type, t_collect when it is still open (it then comes from the body
 * and binds what it mentions). A parameter still mentioning a type parameter of owner is E0343 naming it
 * (`what`: the callee or the case), and the function is checked with error-typed parameters. */
static Type *lambda_expected_in(Type *known, Node *owner, Node *fnode, Str *what)
{
    if (!known || known->kind != TY_FUNC) return known;
    for (long i = 0; i < known->a.v->n; i++) {
        Type *open = open_param_in(vec_get(known->a.v, i), owner);
        if (!open) continue;
        if (lambda_written(fnode)) return 0; /* 0.4 code: its written types stand (E0342 without a fix) */
        ck_err("E0343", fnode->a.n, msg("`$1` of `$2` is known only from this function's parameters: pass a named "
                                        "function, or give a binding its type.", ty_name(open), what, 0));
        return t_error;
    }
    Type *ret = ty_mentions_owner(known->b.t, owner) ? t_collect : known->b.t;
    return ty_func(known->a.v, ret, es_new(0));
}

/* A call of a generic function: arguments, inference, constraints, effects; the return type. */
Type *check_generic_call(Node *call, Node *decl, Type *expected)
{
    Vec *tparams = ty_tparams_of(decl);
    Type **b = alloc(tparams->n * 8); /* bindings */
    if (!check_argc(call, decl->b.v->n)) return t_error;
    if (expected) { /* spec 029 (R10): parameters the expected type determines are bound first, so function
                       values with fewer effects fit (checked again below, E0502 otherwise) */
        Type **eb = alloc(tparams->n * 8);
        Node *outer = ck_infer_owner; /* (checked as a later argument of another generic call: its own
                                         parameters are still open in expected; what does not mention them is
                                         kept even when the rest does not match) */
        ck_infer_owner = 0;
        int all = ty_unify(decl->type, expected, decl, eb);
        if (all || outer)
            for (long i = 0; i < tparams->n; i++)
                if (eb[i] && !ty_bad(eb[i]) && !(outer && ty_mentions_owner(eb[i], outer))) b[i] = eb[i];
    }
    long unified = 0; /* bit i: argument i unified (spec 013: only those are checked again) */
    long deferred = 0; /* bit i: argument i takes its type from the parameter (spec 014, R8): checked last */
    for (long pass = 0; pass < 2; pass++) {
        if (pass && expected) ty_unify(decl->type, expected, decl, b); /* it can bind what the arguments did not */
        for (long i = 0; i < decl->b.v->n; i++) { /* each argument: unify its type with the parameter's */
            Type *pt = ((Node *)vec_get(decl->b.v, i))->type;
            Node *an = call->b.v->data[i].n;
            /* spec 059 (R3): an untyped anonymous function with parameters is checked after the others; one
               without parameters needs nothing from them and keeps its place (spec 013's C2 order) */
            int lambda = an->kind == N_LAMBDA && an->b.v->n; /* (also 0.4 ones: E0342 compares) */
            if (!pass && i < 63 && ty_has_params(pt) && (needs_context(an) || lambda)) {
                deferred |= 1L << i;
                continue;
            }
            if (pass && (i >= 63 || !(deferred >> i & 1))) continue;
            Type *want = ty_has_params(pt) ? 0 : pt; /* nothing to infer: check directly */
            if (!want && !pass) { /* spec 029: what the expected type already bound makes it known */
                Type *known = ty_subst_bindings(pt, decl, b);
                if (!ty_mentions_owner(known, decl) && !ty_has_func(known)) want = known;
            }
            if (pass) { /* the parameter's type as far as the other arguments made it known */
                Type *known = ty_subst_bindings(pt, decl, b);
                if (!ty_mentions_owner(known, decl)) want = known;
            }
            /* spec 028: a generic function named as a value is instantiated from the parameter's type; its
               effects bind the parameter's effect parameter (checked again below, as function values are) */
            Type *at;
            if (!pass && an->kind == N_LAMBDA && !an->b.v->n) { /* spec 059: without parameters, in its place */
                at = check_expr(an, lambda_expected_in(ty_subst_bindings(pt, decl, b), decl, an, call->a.n->a.s));
            } else if (pass && lambda) { /* spec 059 (R3): the parameter as far as the arguments before made it known */
                Type *le = lambda_expected_in(ty_subst_bindings(pt, decl, b), decl, an, call->a.n->a.s);
                if (le == t_error) /* E0343 named what is open: no E0310 for it as well */
                    for (long k = 0; k < tparams->n; k++)
                        if (!b[k]) b[k] = t_error;
                at = check_expr(an, le);
            } else if (pass && (an->kind == N_IDENT || an->kind == N_CALL)) { /* spec 029: a generic value or call that
                                                                          takes its type from this parameter */
                Node *saved = ck_infer_owner;
                ck_infer_owner = decl;
                at = check_expr(an, ty_subst_bindings(pt, decl, b));
                ck_infer_owner = saved;
            } else {
                at = check_arg(call, i, want);
            }
            if (ty_unify(pt, at, decl, b)) {
                if (i < 63) unified |= 1L << i;
                continue;
            }
            Type *known = ty_subst_bindings(pt, decl, b); /* the parameter's type as far as it is known */
            ck_mismatch(an, known, at);
        }
    }
    if (!bind_rest(call, tparams, b, call->a.n->a.s))     /* what stayed unbound */
        infer_fix(call, diag_last()); /* E0310: an annotation fix when a later use shows the type */
    for (long i = 0; i < tparams->n; i++) { /* `ordered` parameters */
        Type *t = b[i];
        Node *tp = vec_get(tparams, i);
        if (tp->b.i != TPC_EFFECTS) check_targ(t, tp, call, call->a.n->a.s);
    }
    Vec *targs = bindings_vec(b, tparams->n); /* bindings for the runtime */
    call->e.v = targs;
    for (long i = 0; i < decl->b.v->n; i++) { /* function values, also nested (spec 013, C2), must fit */
        Type *want = ty_subst(((Node *)vec_get(decl->b.v, i))->type, decl, targs);
        Node *arg = vec_get(call->b.v, i);
        if ((i >= 63 || (unified >> i & 1)) && arg->type && ty_has_func(arg->type)) expect_type(arg, want, arg->type);
    }
    EffSet *declared = decl->a.n->b.fx->declared; /* the declaration with its effect parameters bound */
    if (declared && file_get((int)decl->file)->std) { /* spec 011: as for non-generic library calls (fx_call), */
        EffSet *params = es_new(0);                   /* its own effects are needed in any scope, and its */
        params->evars = declared->evars;              /* effect parameters as bound at this call */
        fx_value_call(es_subst(params, decl, targs));
        if (ck_fx && ck_fx->direct) ck_fx->direct->any |= declared->mask;
    } else if (declared) {
        fx_value_call(es_subst(declared, decl, targs));
    }
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
        Node *pv = node->c.n; /* spec 059: an anonymous function gets the payload type as far as it is known */
        Type *at = check_expr(pv, pv->kind == N_LAMBDA
                                      ? lambda_expected_in(ty_subst_bindings(payload_type_node->type, decl, b), decl, pv,
                                                           node->b.n->a.s)
                                      : 0);
        if (!ty_unify(payload_type_node->type, at, decl, b)) {
            Type *known = ty_subst_bindings(payload_type_node->type, decl, b);
            ck_mismatch(node->c.n, known, at);
        } else if (ty_has_func(at)) { /* spec 013 (C2): effects of function values in the payload */
            expect_type(node->c.n, ty_subst_bindings(payload_type_node->type, decl, b), at);
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
    if (t->b.i == TPC_ORDERED || t->b.i == TPC_KEY) return 1; /* spec 056: every key type is ordered */
    if (t->b.i != TPC_NONE) { /* a parameter takes one bound: `: ordered` added after `: json` would not parse */
        ck_err("E0311", node, msg("`$1` is declared `json`, which has no order; a type parameter takes one bound, so it cannot also be `ordered`.",
                                  ty_param_name(t), 0, 0));
        return 2;
    }
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
