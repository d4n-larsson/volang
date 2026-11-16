/* Effect checking per function and test (research §3, §10; ported from src/check_effects.s,
 * spec 008).
 *
 * Each function and test has a side record FX (data-model.md): the declared set (from `uses`),
 * the direct set (builtins called in the body, as "any scope" bits) and the computed requirement.
 * Calls to user functions are recorded as edges while bodies are checked; after a file's bodies,
 * the requirement is the least fixpoint need(f) = direct(f) ∪ ⋃ (declared(g) ∪ need(g)) over its
 * calls f→g. Imports are checked before their importers, so imported requirements are final. */
#include "vo.h"

Fx *ck_fx;             /* FX of the function or test being checked, or 0 */
static Vec *ck_edges;  /* call edges of the current file */
static Vec *all_edges; /* call edges of the whole program */
static Vec *ck_lits;   /* spec 013 (L1): {fx, literal node, bit} for literal targets of the current file */

/* A call edge: {caller FX, callee decl, call node}. */
typedef struct Edge {
    Fx *caller;
    Node *callee;
    Node *call;
} Edge;
CHECK_SIZE(Edge, 24);

/* Effect bit per builtin id (check_expr.c builtin_names); 0 = pure. */
const unsigned short builtin_effects[64] = {
    [1] = EF_CONSOLE, [2] = EF_CONSOLE, [3] = EF_CONSOLE, [4] = EF_INPUT, [5] = EF_INPUT,
    [6] = EF_FREAD,   [7] = EF_FWRITE,  [10] = EF_CLOCK,  [40] = EF_ENV,  [41] = EF_RANDOM,
    [53] = EF_NET, /* __http_exchange */
    [56] = EF_SERVE, [57] = EF_SERVE, [58] = EF_SERVE, /* spec 011: __serve_listen, __serve_next, __serve_reply */
};

#define M_MISSING "`$1` does not declare $2, which it needs."
#define M_NARROW "`$1` declares too narrow a scope: it needs $2."

/* The FX record of a function or test, or 0 (anything else, or a function without a name). */
Fx *item_fx(Node *item)
{
    if (item->kind == N_FN || item->kind == N_LAMBDA) /* anonymous functions have the same layout */
        return item->a.n ? item->a.n->b.fx : 0;
    if (item->kind != N_TEST) return 0;
    if (!item->d.fx) {
        Fx *fx = alloc(FX_SIZE);
        item->d.fx = fx;
        fx->uses = item->c.n;
    }
    return item->d.fx;
}

/* Builds the declared set from the `uses` clause. */
void fx_declare(Node *item)
{
    Fx *fx = item_fx(item);
    if (!fx) return;
    fx->direct = es_new(0);
    fx->required = 0;
    fx->declared = fx_build_set(fx->uses, fx);
}

/* The set a `uses` clause (or 0) declares: E0506 for invalid scopes, E0509 for unknown names
 * (and FX_BAD on fx, when given, so its effect reports are skipped). */
EffSet *fx_build_set(Node *uses, Fx *fx)
{
    EffSet *set = es_new(0);
    if (!uses) return set;
    Vec *effects = uses->a.v;
    for (long i = 0; i < effects->n; i++) {
        Node *eff = vec_get(effects, i);
        long bit = eff->a.i;
        if (!bit) {
            if (eff->c.n) { /* an effect parameter in scope? */
                Sym *s = scope_find(eff->c.n->a.s);
                if (s && s->kind == SYM_TPARAM && s->type->b.i == TPC_EFFECTS) {
                    es_add_evar(set, s->type);
                    continue;
                }
            }
            if (fx) fx->bad = 1;
            fx_unknown_effect(eff);
            continue;
        }
        Vec *scopes = eff->b.v;
        if (!scopes) {
            set->mask |= (unsigned long)bit;
            continue;
        }
        for (long j = 0; j < scopes->n; j++) {
            Node *sc = vec_get(scopes, j);
            if (es_add_scope(set, bit, sc->a.s)) continue;
            if (fx) fx->bad = 1;
            if (bit == EF_SERVE)
                ck_err1("E0506", sc, "\"$1\" is not a valid serve address: write \"PORT\" or \"0.0.0.0:PORT\".", sc->a.s);
            else
                ck_err1("E0506", sc,
                        "\"$1\" is not a valid scope: write a relative directory without `..`, or a non-empty name.",
                        sc->a.s);
        }
    }
    return set;
}

/* E0509, with a fix to the closest effect name within edit distance 2. */
void fx_unknown_effect(Node *eff)
{
    Str *name = eff->d.s;
    Diag *d = ck_err1(
        "E0509", eff, "`$1` is not an effect: use console, input, file.read, file.write, net, clock, random, env or serve.",
        name);
    char *best = 0;
    unsigned long dist = 3;
    for (unsigned long bit = 1; bit <= EF_ALL; bit <<= 1) {
        char *cand = es_bit_name((long)bit);
        unsigned long k = (unsigned long)levenshtein(name->data, name->len, cand, cstr_len(cand), 2);
        if (k >= dist) continue;
        dist = k;
        best = cand;
    }
    if (best) diag_add_fix(d, (int)eff->file, eff->start, eff->start + eff->d.s->len, str_from_cstr(best));
}

/* Effects found while checking the body go to this function's or test's record. */
void fx_begin(Node *item)
{
    ck_fx = item_fx(item);
}

/* Makes fx the record effects go to (anonymous functions); returns the previous one. */
Fx *fx_swap(Fx *fx)
{
    Fx *old = ck_fx;
    ck_fx = fx;
    return old;
}

/* A call through a function value needs the effects of its type. */
void fx_value_call(EffSet *effects)
{
    if (!ck_fx || !effects) return;
    Fx *fx = ck_fx;
    fx->direct = es_union(fx->direct, effects);
}

void fx_end(void)
{
    ck_fx = 0;
}

/* A builtin call needs its effect in some scope. */
void fx_builtin(long id)
{
    if (!ck_fx || !ck_fx->direct) return;
    ck_fx->direct->any |= builtin_effects[id];
}

/* Spec 013 (L1): a string literal as the target (path, name, address) of an effect: checked against
 * the caller's scopes when the file's effects are reported. */
static void fx_literal(Node *call, long bit)
{
    if (!ck_fx || !ck_fx->declared || call->b.v->n < 1) return;
    Node *arg = vec_get(call->b.v, 0);
    if (arg->kind != N_STR) return;
    if (!ck_lits) ck_lits = vec_new();
    Edge *e = alloc(sizeof(Edge));
    e->caller = ck_fx;
    e->callee = arg;
    e->call = (Node *)bit;
    vec_push(ck_lits, e);
}

/* The literal targets of builtins: read_file, write_file, env. */
void fx_builtin_literal(Node *call, long id)
{
    if (id == 6 || id == 7 || id == 40) fx_literal(call, builtin_effects[id]);
}

/* E0503 on each literal target outside the scopes its function declares for that effect. */
static void fx_report_literals(Vec *items)
{
    for (long i = 0; ck_lits && i < ck_lits->n; i++) {
        Edge *e = vec_get(ck_lits, i);
        Fx *fx = e->caller;
        long bit = (long)e->call;
        EffSet *d = fx->declared;
        if (fx->bad || !(d->mask & (unsigned long)bit) || !es_scoped(d, bit)) continue; /* E0501, or any scope */
        Str *lit = e->callee->a.s;
        if (bit & (EF_FREAD | EF_FWRITE)) {
            /* resolved as the run-time check does (links followed, from the working directory), so a
               path the run would allow is never refused here */
            Str *real = rt_resolve(lit);
            int inside = !real;
            for (long k = 0; !inside && k < d->scopes->n; k++) {
                EffScope *sc = vec_get(d->scopes, k);
                Str *dir = sc->bit == bit ? rt_resolve(sc->str) : 0;
                inside = dir && path_under(real, dir);
            }
            if (inside) continue;
        } else if (es_has_pair(d, bit, lit)) { /* env names, serve addresses: exact */
            continue;
        }
        EffSet *r = es_new(0); /* shown as written */
        es_put_pair(r, bit, lit);
        for (long k = 0; k < items->n; k++) {
            Node *item = vec_get(items, k);
            if (item_fx(item) != fx) continue;
            Str *m = msg(M_NARROW, fx_item_name(item), fx_render(r), 0);
            diag_add("E0503", SEV_ERROR, (int)e->callee->file, e->callee->start, e->callee->end, m);
            break;
        }
    }
    ck_lits = 0;
}

/* Records a call from the current function or test to a user function. */
void fx_call(Node *call, Node *decl)
{
    if (!ck_fx || !decl) return;
    Fx *callee = item_fx(decl);
    if (file_get((int)decl->file)->std) { /* spec 007: a standard-library callee counts like a builtin */
        if (!callee || !callee->declared || !ck_fx->direct) return;
        if (str_eq_cstr(decl->a.n->a.s, "serve")) fx_literal(call, EF_SERVE); /* std.http.serve's address */
        ck_fx->direct->any |= callee->declared->mask; /* its effects, needed in any scope */
        return;
    }
    if (!callee) return;
    if (callee->declared && es_evar_count(callee->declared)) return; /* check_generic_call adds them */
    if (!ck_fx->declared) { /* an anonymous function takes the callee's declaration directly */
        fx_value_call(callee->declared);
        return;
    }
    if (!ck_edges) ck_edges = vec_new();
    Edge *e = alloc(sizeof(Edge));
    e->caller = ck_fx;
    e->callee = decl;
    e->call = call;
    vec_push(ck_edges, e);
    if (all_edges) vec_push(all_edges, e);
}

/* A new program: forget the call edges of the previous one. */
void fx_program_begin(void)
{
    all_edges = vec_new();
}

/* The program's call edges: {caller FX, callee decl, call node}. */
Vec *fx_edges(void)
{
    return all_edges;
}

void fx_file_begin(void)
{
    ck_lits = 0;
    ck_edges = vec_new();
}

/* The requirement fixpoint for this file's functions and tests, then their effect diagnostics. */
void fx_file_end(Vec *items)
{
    for (long i = 0; i < items->n; i++) { /* requirement starts as the direct set */
        Fx *fx = item_fx(vec_get(items, i));
        if (fx && fx->direct) fx->required = fx->direct;
    }
    int changed;
    do {
        changed = 0;
        Vec *edges = ck_edges;
        for (long i = 0; i < edges->n; i++) {
            Edge *e = vec_get(edges, i);
            Fx *callee = item_fx(e->callee);
            if (!callee || !callee->declared) continue;
            Fx *caller = e->caller;
            if (!caller->required) continue;
            EffSet *need = callee->required ? callee->required : callee->direct ? callee->direct : callee->declared;
            EffSet *r = es_union(caller->required, es_union(callee->declared, need));
            if (es_equal(r, caller->required)) continue;
            caller->required = r;
            changed = 1;
        }
    } while (changed);
    for (long i = 0; i < items->n; i++) fx_report(vec_get(items, i)); /* report */
    fx_report_literals(items);
}

/* `name` for a function, `test "name"` for a test, as a str. */
Str *fx_item_name(Node *item)
{
    if (item->kind == N_FN) return item->a.n->a.s;
    SB *sb = sb_new();
    sb_cstr(sb, (item->flags & NF_PROP) ? "prop \"" : "test \"");
    if (item->a.s) sb_str(sb, item->a.s);
    sb_cstr(sb, "\"");
    return sb_finish(sb);
}

/* The canonical text of a set as a str. */
Str *fx_render(EffSet *set)
{
    SB *sb = sb_new();
    es_render(set, sb);
    return sb_finish(sb);
}

/* A diagnostic on the item's name (functions) or `test` keyword (tests): "$1" the item, "$2" the set. */
Diag *fx_diag(char *code, int sev, Node *item, char *tmpl, EffSet *set)
{
    Str *rendered = fx_render(set);
    Str *m = msg(tmpl, fx_item_name(item), rendered, 0);
    long start, end;
    if (item->kind == N_FN) {
        start = item->a.n->start;
        end = item->a.n->end;
    } else {
        start = item->start;
        end = start + 4;
    }
    return diag_add(code, sev, (int)item->file, start, end, m);
}

/* A fix that makes the item's `uses` clause declare exactly set (removing the clause when empty). */
void fx_fix_clause(Diag *d, Node *item, EffSet *set)
{
    Node *uses = item_fx(item)->uses;
    Node *body = item->kind == N_FN ? item->d.n : item->b.n; /* N_D of a function, N_B of a test */
    if (!body) return;
    long at = body->start;
    long clause_at = item_fx(item)->clause_at; /* contract clauses follow `uses`: it ends before them */
    if (clause_at) at = clause_at;
    SB *sb = sb_new();
    if (!es_is_empty(set)) {
        sb_cstr(sb, "uses ");
        es_render(set, sb);
        sb_cstr(sb, " ");
    }
    diag_add_fix(d, (int)item->file, uses ? (long)uses->start : at, at, sb_finish(sb));
}

/* E0501 / E0503 when the declaration does not cover the requirement, W0501 for unused effects. */
void fx_report(Node *item)
{
    Fx *fx = item_fx(item);
    if (!fx || fx->bad || !fx->declared || !fx->required) return;
    EffSet *missing = es_missing(fx->declared, fx->required);
    if (!es_is_empty(missing)) {
        int lacks = es_lacks_bits(fx->declared, missing);
        Diag *d = fx_diag(lacks ? "E0501" : "E0503", SEV_ERROR, item, lacks ? M_MISSING : M_NARROW, missing);
        fx_fix_clause(d, item, es_union(fx->declared, fx->required));
        fx_related(d, fx);
        return;
    }
    if (fx->holes) return; /* an unfinished body: its effects are not known yet */
    EffSet *unused = es_unused(fx->declared, fx->required);
    if (es_is_empty(unused)) return;
    Diag *d = fx_diag("W0501", SEV_WARNING, item, "`$1` declares $2, which it never needs.", unused);
    fx_fix_clause(d, item, es_minus(fx->declared, unused));
}

/* One related note per distinct callee whose declaration the caller does not cover. */
void fx_related(Diag *d, Fx *fx)
{
    Vec *edges = ck_edges;
    if (!edges) return;
    for (long i = 0; i < edges->n; i++) {
        Edge *e = vec_get(edges, i);
        if (e->caller != fx) continue;
        Node *callee = e->callee;
        int seen = 0; /* seen before in this caller's edges? */
        for (long j = 0; j < i && !seen; j++) {
            Edge *o = edges->data[j].p;
            seen = o->caller == fx && o->callee == callee;
        }
        if (seen) continue;
        Fx *cf = item_fx(callee);
        if (!cf || !cf->uses || !cf->declared || es_covers(fx->declared, cf->declared)) continue;
        Str *rendered = fx_render(cf->declared);
        Str *m = msg("`$1` declares $2 here.", fx_item_name(callee), rendered, 0);
        Node *u = cf->uses;
        diag_add_related(d, (int)u->file, u->start, u->end, m);
    }
}
