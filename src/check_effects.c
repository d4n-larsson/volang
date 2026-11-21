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
static EffSet *fx_written(Node *uses, Vec *names, long *sets);

/* A call edge: {caller FX, callee decl, call node}. */
typedef struct Edge {
    Fx *caller;
    Node *callee;
    Node *call;
} Edge;
CHECK_SIZE(Edge, 24);

/* Effect bit per builtin id (check_expr.c builtin_names); 0 = pure. */
const unsigned short builtin_effects[B_LAST + 1] = {
    [1] = EF_CONSOLE, [2] = EF_CONSOLE, [3] = EF_CONSOLE, [4] = EF_INPUT, [5] = EF_INPUT,
    [6] = EF_FREAD,   [7] = EF_FWRITE,  [10] = EF_CLOCK,  [40] = EF_ENV,  [41] = EF_RANDOM,
    [53] = EF_NET, /* __http_exchange */
    [B_HTTPS] = EF_NET, /* spec 019: __https_exchange */
    [56] = EF_SERVE, [57] = EF_SERVE, [58] = EF_SERVE, /* spec 011: __serve_listen, __serve_next, __serve_reply */
    [B_STDIN_BYTES] = EF_INPUT, [B_STDOUT_BYTES] = EF_CONSOLE, [B_RUN_BYTES] = EF_PROCESS, /* spec 021 */
    [B_SLEEP] = EF_CLOCK, /* spec 022 */
    [B_SERVE_WORKERS] = EF_SERVE, /* spec 028 */
    [B_ENTROPY] = EF_ENTROPY, /* spec 035 */
    [B_FS_BLOCK] = EF_FREAD, [B_FS_LINES] = EF_FREAD, [B_PROC_START] = EF_PROCESS, /* spec 040 */
    [B_RUN] = EF_PROCESS, /* spec 017; __fs's effect depends on the operation: checked in src/fs.c */
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
            if (eff->c.n) { /* an effect parameter or an effect set in scope? */
                Sym *s = scope_find(eff->c.n->a.s);
                if (s && s->kind == SYM_TPARAM && s->type->b.i == TPC_EFFECTS) {
                    es_add_evar(set, s->type);
                    continue;
                }
                if (s && s->kind == SYM_EFFECTS) { /* spec 030: its effects, expanded */
                    eff->e.p = s->decl; /* (vo doc: the clause names a set) */
                    q_note_sym(s, eff->c.n, RR_USE);
                    EffSet *sub = fx_set_of(s->decl, eff);
                    if (s->decl->d.i == 3 && fx) fx->bad = 1; /* errors inside it: reported there */
                    set = es_union(set, sub);
                    continue;
                }
            }
            if (fx) fx->bad = 1;
            fx_unknown_effect(eff);
            continue;
        }
        Vec *scopes = eff->b.v;
        if (!scopes) {
            if (bit == EF_PROCESS && !file_get((int)eff->file)->std) { /* spec 017: every program is too much */
                if (fx) fx->bad = 1;
                ck_err("E0512", eff, str_from_cstr("`process` needs the programs it may run: write `process(\"NAME\")`."));
                continue;
            }
            set->mask |= (unsigned long)bit;
            continue;
        }
        for (long j = 0; j < scopes->n; j++) {
            Node *sc = vec_get(scopes, j);
            if (!const_literal(sc, t_str)) { /* spec 031: a constant expression, reported there */
                if (fx) fx->bad = 1;
                continue;
            }
            if (es_add_scope(set, bit, sc->a.s)) continue;
            if (fx) fx->bad = 1;
            int control = 0; /* spec 042: a scope is one line of vo.effects */
            for (long k = 0; k < sc->a.s->len; k++) control |= (unsigned char)sc->a.s->data[k] < 0x20 || sc->a.s->data[k] == 0x7f;
            if (control) {
                ck_err("E0506", sc, str_from_cstr("A scope cannot hold a line break or another control character: it is written on one line of vo.effects."));
                continue;
            }
            if (bit == EF_SERVE)
                ck_err1("E0506", sc, "\"$1\" is not a valid serve address: write \"PORT\" or \"0.0.0.0:PORT\".", sc->a.s);
            else if (bit == EF_PROCESS)
                ck_err1("E0506", sc, "\"$1\" is not a valid program: write a name, or a path without `..`.", sc->a.s);
            else if (bit == EF_NET && sc->a.s->len)
                ck_err1("E0506", sc, "\"$1\" is not a valid host: write \"HOST\" or \"HOST:PORT\" with a port from 1 to 65535.", sc->a.s);
            else
                ck_err1("E0506", sc,
                        "\"$1\" is not a valid scope: write a relative directory without `..`, or a non-empty name.",
                        sc->a.s);
        }
    }
    return set;
}

/* Spec 030: the sets being expanded (cycle check) and the expanded set of an `effects` item: N_C the
 * set, N_D 0 not yet, 1 expanding, 2 done, 3 done with errors inside; N_E 1 once something names it. */
static Vec *fx_expanding;

EffSet *fx_set_of(Node *decl, Node *at)
{
    if (at) decl->e.i = 1;
    if (decl->d.i >= 2) return decl->c.p;
    if (decl->d.i == 1) { /* a cycle: E0513 at the name that closes it */
        SB *sb = sb_new();
        long from = 0;
        while (from < fx_expanding->n && fx_expanding->data[from].n != decl) from++;
        for (long i = from; i < fx_expanding->n; i++) {
            sb_str(sb, ((Node *)vec_get(fx_expanding, i))->a.n->a.s);
            sb_cstr(sb, " -> ");
        }
        sb_str(sb, decl->a.n->a.s);
        ck_err1("E0513", at, "Effect sets include each other in a cycle: $1.", sb_finish(sb));
        decl->d.i = 3;
        return es_new(0);
    }
    if (!fx_expanding) fx_expanding = vec_new();
    decl->d.i = 1;
    vec_push(fx_expanding, decl);
    Fx bad = {0};
    EffSet *set = fx_build_set(decl->b.n, &bad);
    fx_expanding->n--;
    decl->c.p = set;
    if (decl->d.i == 1) decl->d.i = bad.bad ? 3 : 2;
    return set;
}

/* Expands the file's sets once, before any clause names them (their errors reported here, once). */
void fx_expand_sets(Vec *items)
{
    fx_expanding = vec_new();
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_EFFECTS && it->a.n && it->b.n) fx_set_of(it, 0);
    }
}

/* Spec 030: 1 when the clause names an effect set. */
int fx_names_set(Node *uses)
{
    for (long i = 0; uses && i < uses->a.v->n; i++)
        if (((Node *)vec_get(uses->a.v, i))->e.p) return 1;
    return 0;
}

/* Spec 030: a valid clause as written, canonical: its effects, then its names (parameters, sets) sorted. */
void fx_render_written(SB *sb, Node *uses)
{
    Vec *names = vec_new();
    long sets;
    EffSet *plain = fx_written(uses, names, &sets);
    if (!es_is_empty(plain)) es_render(plain, sb);
    for (long i = 0; i < names->n; i++) {
        if (i || !es_is_empty(plain)) sb_cstr(sb, ", ");
        sb_str(sb, ((Node *)vec_get(names, i))->d.s);
    }
}

/* E0509, with a fix to the closest effect name within edit distance 2. */
void fx_unknown_effect(Node *eff)
{
    Str *name = eff->d.s;
    if (eff->c.n) { /* spec 030: a capitalized name; the fix is the nearest effect parameter or set */
        Diag *d = ck_err1("E0509", eff, "`$1` is neither an effect parameter nor an effect set in scope.", name);
        Str *best = 0;
        long dist = 3;
        for (Sym *s = ck_scope; s; s = s->next) {
            if (s->kind != SYM_EFFECTS && !(s->kind == SYM_TPARAM && s->type->b.i == TPC_EFFECTS)) continue;
            long k = levenshtein(name->data, name->len, s->name->data, s->name->len, 2);
            if (k < dist) dist = k, best = s->name;
        }
        if (best) diag_add_fix(d, (int)eff->file, eff->start, eff->end, best);
        return;
    }
    Diag *d = ck_err1(
        "E0509", eff, "`$1` is not an effect: use console, input, file.read, file.write, file.delete, net, clock, random, env, serve or process.",
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

/* Spec 017: the effect bits of __fs operation op (1-6), or 0. */
long fs_op_effects(long op)
{
    static const unsigned short bits[] = {0, EF_FREAD, EF_FREAD, EF_FWRITE, EF_FWRITE, EF_FDELETE, EF_FWRITE | EF_FDELETE};
    return op >= 1 && op <= 6 ? bits[op] : 0;
}

/* Spec 017: __fs(OP, ...) in std/fs.vo needs the effect of its literal operation, in some scope. */
void fx_builtin_fs(Node *call)
{
    if (!ck_fx || !ck_fx->direct || call->b.v->n < 1) return;
    Node *op = vec_get(call->b.v, 0);
    if (op->kind == N_INT) ck_fx->direct->any |= (unsigned long)fs_op_effects(op->a.i);
}

/* spec 021: __fsb(op, ...): 1 reads (file.read), 2 and 3 write (file.write). */
void fx_builtin_fsb(Node *call)
{
    if (!ck_fx || !ck_fx->direct || call->b.v->n < 1) return;
    Node *op = vec_get(call->b.v, 0);
    if (op->kind == N_INT) ck_fx->direct->any |= op->a.i == 1 ? EF_FREAD : EF_FWRITE;
}

/* spec 026: __sql_open(path, mode): 0 read-write (file.read, file.write), 1 read-only (file.read), 2 in
 * memory (none); the effects of using a database come from its type (std.sqlite). */
void fx_builtin_sql_open(Node *call)
{
    if (!ck_fx || !ck_fx->direct || call->b.v->n < 2) return;
    Node *mode = vec_get(call->b.v, 1);
    if (mode->kind == N_INT) ck_fx->direct->any |= mode->a.i == 0 ? EF_FREAD | EF_FWRITE : mode->a.i == 1 ? EF_FREAD : 0;
}

/* spec 039: __tcp(op, a, text): connecting and starting TLS (1, 2, 5) need net; listening, accepting
 * and closing a listener (3, 4, 8) serve; the effects of using a connection come from its type. */
void fx_builtin_tcp(Node *call)
{
    if (!ck_fx || !ck_fx->direct || call->b.v->n < 1) return;
    Node *op = vec_get(call->b.v, 0);
    if (op->kind == N_INT) ck_fx->direct->any |= op->a.i <= 2 || op->a.i == 5 ? EF_NET : op->a.i <= 4 || op->a.i == 8 ? EF_SERVE : 0;
}

/* Spec 013 (L1): a string literal as the target (path, name, address) of an effect: checked against
 * the caller's scopes when the file's effects are reported. */
static void fx_literal_at(Node *call, long index, long bit)
{
    if (!ck_fx || !ck_fx->declared || call->b.v->n <= index) return;
    Node *arg = vec_get(call->b.v, index);
    if (arg->kind != N_STR && arg->kind != N_IDENT && arg->kind != N_BINARY) return; /* spec 031: or a constant expression (folded when reported) */
    if (!ck_lits) ck_lits = vec_new();
    Edge *e = alloc(sizeof(Edge));
    e->caller = ck_fx;
    e->callee = arg;
    e->call = (Node *)bit;
    vec_push(ck_lits, e);
}

static void fx_literal(Node *call, long bit)
{
    fx_literal_at(call, 0, bit);
}

/* Spec 017: the library functions whose arguments are effect targets: (module file, name, index, bit). */
static const struct {
    const char *file, *name;
    long index, bit;
} std_targets[] = {
    {"std/fs.vo", "info", 0, EF_FREAD},          {"std/fs.vo", "list_dir", 0, EF_FREAD},
    {"std/fs.vo", "walk", 0, EF_FREAD},          {"std/fs.vo", "append_file", 0, EF_FWRITE},
    {"std/fs.vo", "make_dir", 0, EF_FWRITE},     {"std/fs.vo", "delete", 0, EF_FDELETE},
    {"std/fs.vo", "rename", 0, EF_FDELETE},      {"std/fs.vo", "rename", 1, EF_FWRITE},
    {"std/process.vo", "run", 0, EF_PROCESS},    {"std/process.vo", "run_with", 0, EF_PROCESS},
};

static void fx_std_literals(Node *call, Node *decl)
{
    Str *path = file_get((int)decl->file)->path;
    for (unsigned long i = 0; i < sizeof std_targets / sizeof std_targets[0]; i++)
        if (str_eq_cstr(path, (char *)std_targets[i].file) && str_eq_cstr(decl->a.n->a.s, (char *)std_targets[i].name))
            fx_literal_at(call, std_targets[i].index, std_targets[i].bit);
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
        if (e->callee->kind != N_STR && !const_fold(e->callee, 0)) continue; /* spec 031: not a constant expression */
        if (e->callee->kind != N_STR) continue;
        Str *lit = e->callee->a.s;
        if (bit & (EF_FREAD | EF_FWRITE | EF_FDELETE)) {
            /* resolved as the run-time check does (links followed, from the working directory), so a
               path the run would allow is never refused here */
            Str *real = rt_resolve_for(bit, lit);
            int inside = !real;
            for (long k = 0; !inside && k < d->scopes->n; k++) {
                EffScope *sc = vec_get(d->scopes, k);
                Str *dir = sc->bit == bit ? rt_resolve(sc->str) : 0;
                inside = dir && path_under(real, dir);
            }
            if (inside) continue;
        } else if (bit == EF_PROCESS) { /* spec 017: programs, exactly after process_name */
            Str *norm = process_name(lit);
            if (norm && es_has_pair(d, bit, norm)) continue;
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
        fx_std_literals(call, decl);                                             /* spec 017 */
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
    for (long i = 0; i < items->n; i++) { /* spec 030: W0505, a private set nothing names */
        Node *it = vec_get(items, i);
        if (it->kind != N_EFFECTS || (it->flags & NF_EXPORT) || it->e.i || !it->a.n) continue;
        Diag *d = ck_warn("W0505", it->a.n, msg("Effect set `$1` is declared but no clause uses it.", it->a.n->a.s, 0, 0));
        FileRec *f = file_get((int)it->file);
        long end = it->end;
        if (end < f->len && f->text[end] == '\n') end++;
        diag_add_fix(d, (int)it->file, it->start, end, str_from_cstr(""));
    }
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

static void fx_fix_text(Diag *d, Node *item, EffSet *set, Vec *names);

/* A fix that makes the item's `uses` clause declare exactly set (removing the clause when empty). */
void fx_fix_clause(Diag *d, Node *item, EffSet *set)
{
    fx_fix_text(d, item, set, 0);
}

/* Spec 030: a clause's written parts: the set of its effects written directly (returned) and, in names,
 * its capitalized names (effect parameters and sets) sorted; *sets counts the sets among them. */
static EffSet *fx_written(Node *uses, Vec *names, long *sets)
{
    Node *tmp = alloc(NODE_SIZE);
    tmp->kind = N_USES;
    tmp->a.v = vec_new();
    *sets = 0;
    for (long i = 0; uses && i < uses->a.v->n; i++) {
        Node *e = vec_get(uses->a.v, i);
        if (!e->c.n) {
            vec_push(tmp->a.v, e);
            continue;
        }
        Sym *s = scope_find(e->c.n->a.s);
        if (s && s->kind == SYM_EFFECTS) (*sets)++;
        long j = names->n;
        vec_push(names, e);
        while (j > 0 && str_cmp(((Node *)names->data[j - 1].p)->d.s, e->d.s) > 0) {
            names->data[j] = names->data[j - 1];
            j--;
        }
        names->data[j].p = e;
    }
    return fx_build_set(tmp, 0);
}

/* `uses SET, Name, ...` (names: N_EFFECT nodes) as the item's clause, or no clause when both are empty. */
static void fx_fix_text(Diag *d, Node *item, EffSet *set, Vec *names)
{
    Node *uses = item_fx(item)->uses;
    Node *body = item->kind == N_FN ? item->d.n : item->b.n; /* N_D of a function, N_B of a test */
    if (!body) return;
    long at = body->start;
    long clause_at = item_fx(item)->clause_at; /* contract clauses follow `uses`: it ends before them */
    if (clause_at) at = clause_at;
    SB *sb = sb_new();
    Node *ret = item->kind == N_FN ? item->c.n : 0;
    long from = uses ? (long)uses->start : at;
    if (!uses && !es_is_empty(set) && ret && ret->kind == N_TYPE_NODE && ret->a.i == TY_FUNC && !ret->d.n) {
        sb_byte(sb, '('); /* a returned function type without its own `uses` would take the clause */
        sb_str(sb, src_text(ret));
        sb_cstr(sb, ") ");
        from = ret->start;
    }
    long nn = names ? names->n : 0;
    if (!es_is_empty(set) || nn) {
        sb_cstr(sb, "uses ");
        if (!es_is_empty(set)) es_render(set, sb);
        for (long i = 0; i < nn; i++) {
            if (i || !es_is_empty(set)) sb_cstr(sb, ", ");
            sb_str(sb, ((Node *)vec_get(names, i))->d.s);
        }
        sb_cstr(sb, " ");
    }
    diag_add_fix(d, (int)item->file, from, at, sb_finish(sb));
}

/* Spec 026: 1 when t carries effect parameter ev as an effect argument of a record or variant
 * (`Database[E]`), at any depth of lists, maps, results and instances. */
static int ty_carries_evar(Type *t, Type *ev)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_PARAM: return t == ev;
    case TY_EFFSET: return es_has_evar(t->decl.es, ev); /* `Database[E]` */
    case TY_LIST:
    case TY_RESULT: return ty_carries_evar(t->a.t, ev) || (t->b.t && ty_carries_evar(t->b.t, ev));
    case TY_MAP: return ty_carries_evar(t->a.t, ev) || ty_carries_evar(t->b.t, ev);
    case TY_RECORD:
    case TY_VARIANT:
        for (long i = 0; t->b.v && i < t->b.v->n; i++)
            if (ty_carries_evar(t->b.v->data[i].p, ev)) return 1;
        return 0;
    default: return 0;
    }
}

/* The effect parameters of unused that a parameter of item carries in its type: the effects come
 * with the value (a database opened with them), so declaring them is the point, not a waste. */
static EffSet *carried_evars(Node *item, EffSet *unused)
{
    EffSet *keep = es_new(0);
    Vec *params = item->b.v;
    for (long i = 0; unused->evars && i < unused->evars->n; i++) {
        Type *ev = unused->evars->data[i].p;
        for (long j = 0; params && j < params->n; j++) {
            Node *p = vec_get(params, j);
            if (p && p->type && ty_carries_evar(p->type, ev)) {
                es_add_evar(keep, ev);
                break;
            }
        }
    }
    return keep;
}

/* E0501 / E0503 when the declaration does not cover the requirement, W0501 for unused effects. */
void fx_report(Node *item)
{
    Fx *fx = item_fx(item);
    if (!fx || fx->bad || !fx->declared || !fx->required) return;
    Vec *names = vec_new();
    long sets = 0;
    EffSet *written = fx_written(fx->uses, names, &sets); /* spec 030: a clause naming sets keeps them */
    EffSet *missing = es_missing(fx->declared, fx->required);
    if (!es_is_empty(missing)) {
        int lacks = es_lacks_bits(fx->declared, missing);
        Diag *d = fx_diag(lacks ? "E0501" : "E0503", SEV_ERROR, item, lacks ? M_MISSING : M_NARROW, missing);
        if (sets)
            fx_fix_text(d, item, es_union(written, missing), names);
        else
            fx_fix_clause(d, item, es_union(fx->declared, fx->required));
        fx_related(d, fx);
        return;
    }
    if (fx->holes) return; /* an unfinished body: its effects are not known yet */
    EffSet *unused = es_unused(fx->declared, fx->required);
    if (es_is_empty(unused)) return;
    if (item->kind == N_FN || item->kind == N_LAMBDA) unused = es_minus(unused, carried_evars(item, unused));
    if (es_is_empty(unused)) return;
    if (sets) { /* W0501 for effects written directly, and for sets (and parameters) nothing needs at all */
        EffSet *direct = es_unused(written, fx->required);
        if (item->kind == N_FN || item->kind == N_LAMBDA) direct = es_minus(direct, carried_evars(item, direct));
        Vec *keep = vec_new();
        SB *dropped = sb_new();
        if (!es_is_empty(direct)) es_render(direct, dropped);
        for (long i = 0; i < names->n; i++) {
            Node *e = vec_get(names, i);
            Sym *s = scope_find(e->c.n->a.s);
            int none = s && s->kind == SYM_EFFECTS ? es_is_empty(es_minus(fx_set_of(s->decl, 0), unused))
                                                   : s && s->kind == SYM_TPARAM && es_has_evar(unused, s->type);
            if (!none) {
                vec_push(keep, e);
                continue;
            }
            if (dropped->len) sb_cstr(dropped, ", ");
            sb_str(dropped, e->d.s);
        }
        if (!dropped->len) return;
        Str *m = msg("`$1` declares $2, which it never needs.", fx_item_name(item), sb_finish(dropped), 0);
        Diag *d = fx_diag("W0501", SEV_WARNING, item, "$1", es_new(0));
        d->msg = m;
        fx_fix_text(d, item, es_minus(written, direct), keep);
        return;
    }
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
