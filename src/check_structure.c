/* Structure rules (spec 027): warnings W0701-W0706 that `vo check` reports for the program's own files
 * (not std, not packages) after a check with no errors.
 *
 *   W0701 effectful_logic    a function with effects has R statements that use no effect (research R3)
 *   W0702 file_too_long      a file has more than F code lines (R1)
 *   W0703 function_too_long  a function, test or property has more than S statements (R2)
 *   W0704 duplicate_code     D consecutive statements repeat earlier ones (R5)
 *   W0705 test_elsewhere     a test calls only functions of one other file (R6)
 *   W0706 unused_allow       an `@allow` that silences nothing (R9)
 *
 * Every warning goes through st_report, which drops one that an `@allow("W07NN", "reason")` on its
 * declaration accepts (counted in the summary) and emits the others. Thresholds are fixed (FR-008). */
#include "vo.h"

#define R_EFFECT_FREE 15 /* research R3: effect-free statements in a function with effects */
#define F_CODE_LINES 400 /* research R1 */
#define S_STATEMENTS 50  /* research R2 */
#define D_DUPLICATE 6    /* research R5: sibling statements */

long diag_accepted; /* warnings an `@allow` accepted (the check summary) */

typedef struct {
    Node *ann;  /* the `@allow` annotation */
    Node *decl; /* the fn or test below it */
    long used;
} Allow;

static Vec *allows;  /* Allow, as check_annotations meets them */
static Vec *done;    /* file ids already analyzed (a file is in several programs: reported once) */

/* Called by check_annotations for each valid `@allow` with its declaration. */
void structure_allow(Node *ann, Node *decl)
{
    if (!allows) allows = vec_new();
    Allow *a = alloc(sizeof(Allow));
    a->ann = ann;
    a->decl = decl;
    vec_push(allows, a);
}

/* The first function, test or property of a file (what `@allow` can stand above): where W0702 is accepted. */
static Node *first_decl(int file)
{
    Vec *items = file_get(file)->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_FN || it->kind == N_TEST) return it;
    }
    return 0;
}

/* Emits warning code at [start, end) of file, unless an `@allow` on decl accepts it; returns the diagnostic
 * (for related locations) or 0. */
static Diag *st_report(char *code, Node *decl, int file, long start, long end, Str *message)
{
    for (long i = 0; allows && i < allows->n; i++) {
        Allow *a = vec_get(allows, i);
        if (a->decl != decl || !str_eq_cstr(((Node *)vec_get(a->ann->b.v, 0))->a.s, code)) continue;
        if (!a->used) diag_accepted++;
        a->used = 1;
        return 0;
    }
    return diag_add(code, SEV_WARNING, file, start, end, message);
}

/* Maintainers' switches, never documented for programs: VO_STRUCTURE_STD=1 (tests/structure_corpus.py,
 * FR-012) counts std files as the program's own, so the rules run on the standard library through a
 * program that imports it; VO_STRUCTURE_OFF=1 (tests/structure_speed.py, SC-004) skips the rules. */
static int env_on(char *setting, long n)
{
    for (char **e = (char **)envp; *e; e++)
        if (mem_eq(*e, setting, n)) return 1;
    return 0;
}

static int std_too(void)
{
    static int known, on;
    if (!known) {
        known = 1;
        on = env_on("VO_STRUCTURE_STD=1", 19);
    }
    return on;
}

/* 1 when file belongs to the program itself (not std, not a package). */
static int own_file(int file)
{
    FileRec *f = file_get(file);
    return !f->pkg && (!f->std || std_too());
}

/* ---- statements and their effects (data-model.md "Statement") ---- */

typedef struct Count Count;
static int expr_effect(Node *e, Count *c);
static int walk_block(Node *block, Count *c);

static int vec_effect(Vec *v, Count *c)
{
    int any = 0;
    for (long i = 0; v && i < v->n; i++) any |= expr_effect(vec_get(v, i), c);
    return any;
}

/* 1 when a value of type t is a function that has effects. */
static int fn_type_effect(Type *t)
{
    return t && t->kind == TY_FUNC && !es_is_empty(t->decl.es);
}

/* 1 when evaluating expression e requires an effect: a call of an effectful builtin, of a function with
 * effects (an effect-generic one: with its effects at that call), of a function value whose type has
 * effects, or a builtin given such a function value (`map(xs, f)`). Creating an anonymous function is
 * not an effect. */
static int expr_effect(Node *e, Count *c)
{
    if (!e) return 0;
    switch (e->kind) {
    case N_LAMBDA: /* creating it is no effect; its body's statements are the function's, counted (one walk) */
        if (c) walk_block(e->d.n, c);
        return 0;
    case N_CALL: {
        int any = vec_effect(e->b.v, c);
        if (e->d.i) { /* a builtin */
            if (builtin_effects[e->d.i]) return 1;
            for (long i = 0; e->b.v && i < e->b.v->n; i++) any |= fn_type_effect(((Node *)vec_get(e->b.v, i))->type);
            return any;
        }
        Node *decl = e->c.n;
        Fx *fx = decl ? item_fx(decl) : 0;
        if (!fx || !fx->declared) return any;
        EffSet *s = fx->declared;
        if (es_evar_count(s) && e->e.v) s = es_subst(s, decl, e->e.v); /* as bound at this call */
        return any || !es_is_empty(s);
    }
    case N_CALL_VALUE: return expr_effect(e->a.n, c) | vec_effect(e->b.v, c) | fn_type_effect(e->a.n ? e->a.n->type : 0);
    case N_FIELD:
    case N_UNARY:
    case N_TRY: return expr_effect(e->a.n, c);
    case N_INDEX:
    case N_BINARY: return expr_effect(e->a.n, c) | expr_effect(e->b.n, c);
    case N_VCASE: return expr_effect(e->c.n, c);
    case N_LIST: return vec_effect(e->a.v, c);
    case N_NEW_RECORD: return vec_effect(e->b.v, c);
    case N_FINIT: return expr_effect(e->b.n, c);
    default: return 0;
    }
}

/* 1 when a statement's own expressions (a compound statement's condition, iterable or scrutinee) require
 * an effect; its nested blocks are statements of their own (research R4). */
static int own_effect(Node *st, Count *c)
{
    switch (st->kind) {
    case N_LET: return expr_effect(st->c.n, c);
    case N_SET: return expr_effect(st->a.n, c) | expr_effect(st->b.n, c);
    case N_IF:
    case N_WHILE:
    case N_MATCH:
    case N_RETURN:
    case N_EXPR_STMT: return expr_effect(st->a.n, c);
    case N_FOR: return expr_effect(st->b.n, c) | expr_effect(FOR_END(st), c);
    default: return 0;
    }
}

struct Count {
    long stmts;    /* statements, at every depth, anonymous functions' included */
    long free;     /* of them, those whose own expressions use no effect */
    Vec *stretch;  /* Stretch: maximal runs of sibling statements whose whole subtree uses no effect (only
                      recorded for functions with effects, the ones W0701 reports: 0 otherwise) */
};

typedef struct {
    Node *first, *last;
    long n; /* statements in it, at every depth */
} Stretch;

/* Counts statement st (and everything nested); 1 when anything in its subtree, except inside anonymous
 * functions it only creates, uses an effect. */
static int walk_stmt(Node *st, Count *c)
{
    c->stmts++;
    int own = own_effect(st, c), nested = 0; /* (anonymous functions' statements are counted on the way) */
    if (!own) c->free++;
    switch (st->kind) { /* nested blocks */
    case N_IF:
        nested |= walk_block(st->b.n, c);
        if (st->c.n && st->c.n->kind == N_IF) nested |= walk_stmt(st->c.n, c); /* else if */
        else if (st->c.n) nested |= walk_block(st->c.n, c);
        break;
    case N_WHILE: nested |= walk_block(st->b.n, c); break;
    case N_FOR: nested |= walk_block(st->d.n, c); break;
    case N_MATCH:
        for (long i = 0; st->b.v && i < st->b.v->n; i++) nested |= walk_block(((Node *)vec_get(st->b.v, i))->c.n, c);
        break;
    default: break;
    }
    return own || nested;
}

/* Walks a block's statements, recording the maximal stretches of effect-free ones; 1 when any uses an
 * effect. A stretch inside a statement that turns out effect-free as a whole is part of the outer stretch,
 * so it is dropped. */
static int walk_block(Node *block, Count *c)
{
    if (!block || block->kind != N_BLOCK) return 0;
    Vec *v = block->a.v;
    int any = 0;
    Stretch *cur = 0;
    for (long i = 0; i < v->n; i++) {
        Node *st = vec_get(v, i);
        if (st->kind == N_COMMENT) continue;
        long before = c->stmts, mark = c->stretch ? c->stretch->n : 0;
        int eff = walk_stmt(st, c);
        any |= eff;
        if (eff) {
            cur = 0;
            continue;
        }
        if (!c->stretch) continue;
        c->stretch->n = mark; /* nested stretches are inside this one */
        if (!cur) {
            cur = alloc(sizeof(Stretch));
            cur->first = st;
            vec_push(c->stretch, cur);
        }
        cur->last = st;
        cur->n += c->stmts - before;
    }
    return any;
}

/* ---- the rules (one section per user story) ---- */

/* The body of a function or test, and its statement counts (with stretches when record). */
static Count count_body(Node *body, int record)
{
    Count c = {0, 0, record ? vec_new() : 0};
    walk_block(body, &c);
    return c;
}

/* W0701 (User Story 1): a function with effects whose statements mostly use none. */
static void rule_effectful_logic(Node *fn, Count *c)
{
    Fx *fx = item_fx(fn);
    if (!fx || !fx->declared || es_is_empty(fx->declared) || c->free < R_EFFECT_FREE) return;
    Str *name = fn->a.n->a.s;
    Diag *d = st_report("W0701", fn, (int)fn->file, fn->a.n->start, fn->a.n->end,
                        msg("`$1` has $2 statements that use no effect: move that logic into pure functions, which "
                            "tests can call without effects, and keep `$1` to the effects.",
                            name, int_to_str(c->free), 0));
    if (!d) return;
    for (int k = 0; k < 3; k++) { /* the three longest stretches, longest first, then by position */
        Stretch *best = 0;
        for (long i = 0; i < c->stretch->n; i++) {
            Stretch *s = vec_get(c->stretch, i);
            if (s->n > 0 && (!best || s->n > best->n)) best = s;
        }
        if (!best) break;
        diag_add_related(d, (int)best->first->file, best->first->start, best->last->end,
                         best->n == 1 ? str_from_cstr("1 statement that uses no effect")
                                      : msg("$1 statements that use no effect", int_to_str(best->n), 0, 0));
        best->n = -best->n; /* taken */
    }
}

/* W0702 (User Story 2): a file with more than F code lines (not blank, not only a comment), at line 1;
 * an `@allow` on the file's first declaration accepts it. */
static void rule_file_size(int file)
{
    FileRec *f = file_get(file);
    long n = 0, state = 0; /* this line: 0 nothing yet, 1 code, 2 a comment (its first character decides) */
    for (long i = 0; i <= f->len; i++) {
        if (i == f->len || f->text[i] == '\n') {
            n += state == 1;
            state = 0;
        } else if (!state && f->text[i] != ' ' && f->text[i] != '\t' && f->text[i] != '\r') {
            state = f->text[i] == '#' ? 2 : 1;
        }
    }
    if (n <= F_CODE_LINES) return;
    st_report("W0702", first_decl(file), file, 0, 0,
              msg("This file has $1 code lines, more than 400: split it into modules, one concept each, and import "
                  "what `main.vo` needs.",
                  int_to_str(n), 0, 0));
}

/* W0703 (User Story 2): a function, test or property with more than S statements. */
static void rule_function_size(Node *it, Count *c)
{
    if (c->stmts <= S_STATEMENTS) return;
    Str *name = it->kind == N_FN ? it->a.n->a.s : fx_item_name(it);
    long start = it->kind == N_FN ? it->a.n->start : it->start, end = it->kind == N_FN ? it->a.n->end : it->start + 4;
    st_report("W0703", it, (int)it->file, start, end,
              msg("`$1` has $2 statements, more than 50: split it into functions that each do one thing.", name,
                  int_to_str(c->stmts), 0));
}

/* ---- W0704 (User Story 3): duplicated statements (data-model.md "Duplicate run") ---- */

/* The normal form of code: tokens of the tree, each local name as its position of first appearance
 * (names), each literal as its kind; call targets, fields, case names, operators and types as written. */
/* The serializer's output: an SB, or HASHED (then the bytes only update out_hash: shapes and windows are
 * compared by hash, and spelled out as strs only to confirm an equal hash). */
#define HASHED ((SB *)1)
static unsigned long out_hash;

static void o_bytes(SB *sb, char *p, long n)
{
    if (sb != HASHED) {
        sb_bytes(sb, p, n);
        return;
    }
    unsigned long h = out_hash;
    for (long i = 0; i < n; i++) h = (h ^ (unsigned char)p[i]) * 1099511628211UL;
    out_hash = h;
}

static void o_byte(SB *sb, char c)
{
    o_bytes(sb, &c, 1);
}

static void o_cstr(SB *sb, char *s)
{
    long n = 0;
    while (s[n]) n++;
    o_bytes(sb, s, n);
}

static void o_dec(SB *sb, long v)
{
    char buf[24];
    long n = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do buf[23 - n++] = (char)('0' + u % 10);
    while ((u /= 10) && n < 23);
    if (v < 0) buf[23 - n++] = '-';
    o_bytes(sb, buf + 24 - n, n);
}

static void norm(SB *sb, Node *n, Vec *names);
static int keep_literals; /* effect-only copies compare literal values too (only their effects differ) */
static int anon_locals;   /* shapes: every local name as one placeholder (a cheap, necessary condition) */

/* The text of a name node, or 0 (`for _ in`, `case x`, ...). */
static Str *nm(Node *n)
{
    return n ? n->a.s : 0;
}

static void norm_local(SB *sb, Str *name, Vec *names)
{
    if (!name || anon_locals) {
        o_cstr(sb, "_ ");
        return;
    }
    long i = 0;
    while (i < names->n && !str_eq(vec_get(names, i), name)) i++;
    if (i == names->n) vec_push(names, name);
    o_byte(sb, 'L');
    o_dec(sb, i);
    o_byte(sb, ' ');
}

static void norm_vec(SB *sb, Vec *v, Vec *names)
{
    o_byte(sb, '(');
    for (long i = 0; v && i < v->n; i++) norm(sb, vec_get(v, i), names);
    o_byte(sb, ')');
}

static void norm_word(SB *sb, char *w, Str *s)
{
    o_cstr(sb, w);
    if (s) o_bytes(sb, s->data, s->len);
    o_byte(sb, ' ');
}

/* A node's source text into the normal form, without a copy. */
static void norm_src(SB *sb, char *w, Node *n)
{
    FileRec *f = file_get((int)n->file);
    o_cstr(sb, w);
    o_bytes(sb, f->text + n->start, (long)n->end - n->start);
    o_byte(sb, ' ');
}

static void norm(SB *sb, Node *n, Vec *names)
{
    if (!n) {
        o_cstr(sb, "_ ");
        return;
    }
    switch (n->kind) {
    case N_BLOCK: norm_vec(sb, n->a.v, names); return;
    case N_COMMENT: return;
    case N_LET:
        norm_word(sb, n->flags & NF_MUT ? "var" : "let", 0);
        norm_local(sb, nm(n->a.n), names);
        if (n->b.n) norm_src(sb, ":", n->b.n);
        norm(sb, n->c.n, names);
        return;
    case N_SET:
        norm_word(sb, "set", 0);
        norm(sb, n->a.n, names);
        norm(sb, n->b.n, names);
        return;
    case N_IF:
        norm_word(sb, "if", 0);
        norm(sb, n->a.n, names);
        norm(sb, n->b.n, names);
        norm(sb, n->c.n, names);
        return;
    case N_WHILE:
        norm_word(sb, "while", 0);
        norm(sb, n->a.n, names);
        norm(sb, n->b.n, names);
        return;
    case N_FOR:
        norm_word(sb, "for", 0);
        norm_local(sb, nm(n->a.n), names);
        if (n->flags & NF_PAIR) norm_local(sb, nm(n->c.n), names); /* spec 046 */
        norm(sb, n->b.n, names);
        norm(sb, FOR_END(n), names);
        norm(sb, n->d.n, names);
        return;
    case N_MATCH:
        norm_word(sb, "match", 0);
        norm(sb, n->a.n, names);
        norm_vec(sb, n->b.v, names);
        return;
    case N_ARM:
        norm_word(sb, "case ", nm(n->a.n));
        if (n->b.n) norm_local(sb, nm(n->b.n), names);
        norm(sb, n->c.n, names);
        return;
    case N_RETURN:
        norm_word(sb, "return", 0);
        norm(sb, n->a.n, names);
        return;
    case N_BREAK: norm_word(sb, "break", 0); return;
    case N_CONTINUE: norm_word(sb, "continue", 0); return;
    case N_EXPR_STMT: norm(sb, n->a.n, names); return;
    case N_INT:
    case N_FLOAT:
    case N_STR:
    case N_BOOL:
        if (keep_literals) norm_src(sb, "#", n);
        else norm_word(sb, n->kind == N_INT ? "#int" : n->kind == N_FLOAT ? "#float" : n->kind == N_STR ? "#str" : "#bool", 0);
        return;
    case N_IDENT: {
        Sym *s = n->b.p;
        if (s && s->kind == SYM_LOCAL) norm_local(sb, n->a.s, names);
        else norm_word(sb, "@", n->a.s);
        return;
    }
    case N_CALL:
        norm_word(sb, "call ", nm(n->a.n));
        norm_vec(sb, n->b.v, names);
        return;
    case N_CALL_VALUE:
        norm_word(sb, "callv", 0);
        norm(sb, n->a.n, names);
        norm_vec(sb, n->b.v, names);
        return;
    case N_VCASE:
        norm_word(sb, "case ", nm(n->a.n));
        norm_word(sb, ".", nm(n->b.n));
        norm(sb, n->c.n, names);
        return;
    case N_FIELD:
        norm(sb, n->a.n, names);
        norm_word(sb, ".", nm(n->b.n));
        return;
    case N_INDEX:
        norm(sb, n->a.n, names);
        o_byte(sb, '[');
        norm(sb, n->b.n, names);
        return;
    case N_BINARY:
    case N_UNARY:
        o_cstr(sb, "op");
        o_dec(sb, n->d.i);
        o_byte(sb, ' ');
        norm(sb, n->a.n, names);
        if (n->kind == N_BINARY) norm(sb, n->b.n, names);
        return;
    case N_TRY:
        norm_word(sb, "try", 0);
        norm(sb, n->a.n, names);
        return;
    case N_LIST: norm_vec(sb, n->a.v, names); return;
    case N_NEW_RECORD:
        norm_word(sb, "new ", n->c.n ? nm(n->c.n->a.n) : 0);
        norm_vec(sb, n->b.v, names);
        return;
    case N_FINIT:
        norm_word(sb, "field ", nm(n->a.n));
        norm(sb, n->b.n, names);
        return;
    case N_LAMBDA:
        norm_word(sb, "fn", 0);
        for (long i = 0; n->b.v && i < n->b.v->n; i++) {
            Node *p = vec_get(n->b.v, i);
            norm_local(sb, nm(p->a.n), names);
            if (p->b.n) norm_src(sb, ":", p->b.n);
        }
        if (n->c.n) norm_src(sb, "->", n->c.n);
        norm(sb, n->d.n, names);
        return;
    default: /* types and the rest: as written */
        norm_src(sb, "?", n);
        return;
    }
}

typedef struct {
    unsigned long hash; /* of the shapes of its statements (a necessary condition for equal normal forms) */
    Str *key;           /* its normal form, built when an equal hash needs confirming, then kept */
    Vec *stmts; /* the block's statements (comments left out) */
    long at;    /* the window's first statement in stmts */
    Node *fn;
} Win;

static unsigned long fnv_raw(char *p, long n)
{
    unsigned long h = 1469598103934665603UL;
    for (long i = 0; i < n; i++) h = (h ^ (unsigned char)p[i]) * 1099511628211UL;
    return h;
}

/* One buffer and one name table for every normal form, so hashing allocates nothing (static: in .bss,
 * which the collector scans). */
static Vec *scratch_names;

static SB *scratch_start(void)
{
    if (!scratch_names) scratch_names = vec_new();
    scratch_names->n = 0;
    out_hash = 1469598103934665603UL;
    return HASHED;
}

static unsigned long scratch_hash(void)
{
    return out_hash;
}

/* A statement's shape: its normal form with all locals alike, hashed. Equal windows have equal shapes, so
 * shapes are computed once per statement and only windows of equal shapes are compared exactly. */
static unsigned long shape(Node *st)
{
    SB *sb = scratch_start();
    anon_locals = 1;
    norm(sb, st, scratch_names);
    anon_locals = 0;
    return scratch_hash();
}

/* The normal form of a window as a str (to confirm that two equal hashes are equal forms). */
static Str *win_key(Vec *stmts, long at)
{
    SB *sb = sb_new();
    Vec *names = vec_new();
    for (long k = 0; k < D_DUPLICATE; k++) norm(sb, vec_get(stmts, at + k), names);
    return sb_finish(sb);
}

static Vec *dup_wins; /* Win, in program order */

static void block_windows(Node *block, Node *fn);

/* The blocks inside statement st (nested blocks, anonymous functions' bodies) get their windows too. */
static void stmt_windows(Node *n, Node *fn)
{
    if (!n) return;
    switch (n->kind) {
    case N_BLOCK: block_windows(n, fn); return;
    case N_LAMBDA: block_windows(n->d.n, fn); return;
    case N_IF:
        stmt_windows(n->a.n, fn);
        stmt_windows(n->b.n, fn);
        stmt_windows(n->c.n, fn);
        return;
    case N_WHILE:
        stmt_windows(n->a.n, fn);
        stmt_windows(n->b.n, fn);
        return;
    case N_FOR:
        stmt_windows(n->b.n, fn);
        stmt_windows(n->d.n, fn);
        return;
    case N_MATCH:
        stmt_windows(n->a.n, fn);
        for (long i = 0; n->b.v && i < n->b.v->n; i++) stmt_windows(((Node *)vec_get(n->b.v, i))->c.n, fn);
        return;
    case N_LET: stmt_windows(n->c.n, fn); return;
    case N_SET:
    case N_INDEX:
    case N_BINARY:
        stmt_windows(n->a.n, fn);
        stmt_windows(n->b.n, fn);
        return;
    case N_RETURN:
    case N_EXPR_STMT:
    case N_TRY:
    case N_FIELD:
    case N_UNARY: stmt_windows(n->a.n, fn); return;
    case N_CALL:
    case N_NEW_RECORD:
        for (long i = 0; n->b.v && i < n->b.v->n; i++) stmt_windows(vec_get(n->b.v, i), fn);
        return;
    case N_CALL_VALUE:
        stmt_windows(n->a.n, fn);
        for (long i = 0; n->b.v && i < n->b.v->n; i++) stmt_windows(vec_get(n->b.v, i), fn);
        return;
    case N_LIST:
        for (long i = 0; n->a.v && i < n->a.v->n; i++) stmt_windows(vec_get(n->a.v, i), fn);
        return;
    case N_VCASE: stmt_windows(n->c.n, fn); return;
    case N_FINIT: stmt_windows(n->b.n, fn); return;
    default: return;
    }
}

/* Every window of D consecutive statements of block, then the windows of the blocks inside them. */
static void block_windows(Node *block, Node *fn)
{
    if (!block || block->kind != N_BLOCK) return;
    Vec *all = block->a.v;
    long n = 0;
    for (long i = 0; i < all->n; i++) n += ((Node *)vec_get(all, i))->kind != N_COMMENT;
    if (n >= D_DUPLICATE) {
        Vec *stmts = vec_new();
        for (long i = 0; i < all->n; i++) {
            Node *st = vec_get(all, i);
            if (st->kind != N_COMMENT) vec_push(stmts, st);
        }
        unsigned long *shapes = alloc(stmts->n * sizeof(unsigned long));
        for (long i = 0; i < stmts->n; i++) shapes[i] = shape(vec_get(stmts, i));
        for (long i = 0; i + D_DUPLICATE <= stmts->n; i++) {
            unsigned long h = 1469598103934665603UL;
            for (long k = 0; k < D_DUPLICATE; k++) h = (h ^ shapes[i + k]) * 1099511628211UL;
            Win *w = alloc(sizeof(Win));
            w->hash = h;
            w->stmts = stmts;
            w->at = i;
            w->fn = fn;
            vec_push(dup_wins, w);
        }
    }
    for (long i = 0; i < all->n; i++) stmt_windows(vec_get(all, i), fn);
}

/* "path:line" of a statement, for messages. */
static Str *where(Node *n)
{
    return msg("$1:$2", file_get((int)n->file)->path, int_to_str(pos_line((int)n->file, n->start)), 0);
}

/* A type as written with its effects left out (`Database[uses file.read]` is `Database`, `fn() uses
 * console` is `fn()`), for comparing signatures. */
static Str *without_effects(Node *type)
{
    if (!type) return str_from_cstr("");
    Str *s = src_text(type);
    SB *sb = sb_new();
    for (long i = 0; i < s->len;) {
        if (i + 5 <= s->len && mem_eq(s->data + i, "uses ", 5) && (i == 0 || s->data[i - 1] == '[' || s->data[i - 1] == ' ')) {
            long depth = 0;
            while (i < s->len && !(depth == 0 && (s->data[i] == ']' || s->data[i] == ')'))) {
                depth += (s->data[i] == '(' || s->data[i] == '[') - (s->data[i] == ')' || s->data[i] == ']');
                i++;
            }
            while (sb->len && (sb->ptr[sb->len - 1] == ' ' || sb->ptr[sb->len - 1] == ',')) sb->len--;
            continue;
        }
        sb_byte(sb, s->data[i++]);
    }
    Str *r = sb_finish(sb);
    SB *out = sb_new(); /* `Database[]` -> `Database` */
    for (long i = 0; i < r->len; i++) {
        if (r->data[i] == '[' && i + 1 < r->len && r->data[i + 1] == ']') {
            i++;
            continue;
        }
        sb_byte(out, r->data[i]);
    }
    return sb_finish(out);
}

/* A function's signature with every effect left out. */
static Str *plain_signature(Node *fn)
{
    SB *sb = sb_new();
    for (long i = 0; fn->b.v && i < fn->b.v->n; i++) {
        sb_str(sb, without_effects(((Node *)vec_get(fn->b.v, i))->b.n));
        sb_byte(sb, ',');
    }
    sb_cstr(sb, "->");
    sb_str(sb, without_effects(fn->c.n));
    return sb_finish(sb);
}

/* Effect-only copies: whole functions equal in body and plain signature that differ in effects. Returns
 * the functions reported (their windows are not reported again). */
/* A function's body (parameters numbered as locals) in normal form, into sb. */
static void body_form(SB *sb, Node *fn, Vec *names)
{
    for (long k = 0; fn->b.v && k < fn->b.v->n; k++) norm_local(sb, nm(((Node *)vec_get(fn->b.v, k))->a.n), names);
    norm(sb, fn->d.n, names);
}

/* 1 when two functions have the same body and the same signature without effects (built as strs: only
 * for the rare pairs whose hashes are equal). */
static int same_but_effects(Node *a, Node *b)
{
    SB *sa = sb_new(), *sb = sb_new();
    body_form(sa, a, vec_new());
    body_form(sb, b, vec_new());
    return str_eq(sb_finish(sa), sb_finish(sb)) && str_eq(plain_signature(a), plain_signature(b));
}

/* A type's hash with its effects left out (effect arguments, function types' effects), and in *effects
 * whether it has any: a walk of the interned type, no allocation. */
static unsigned long type_plain(Type *t, int *effects)
{
    if (!t) return 7;
    unsigned long h = (unsigned long)t->kind * 1099511628211UL;
    switch (t->kind) {
    case TY_EFFSET:
        *effects |= !es_is_empty(t->decl.es);
        return 0; /* left out */
    case TY_FUNC:
        *effects |= !es_is_empty(t->decl.es);
        for (long i = 0; t->a.v && i < t->a.v->n; i++) h = (h ^ type_plain(vec_get(t->a.v, i), effects)) * 1099511628211UL;
        return (h ^ type_plain(t->b.t, effects)) * 31;
    case TY_RECORD:
    case TY_VARIANT:
        h ^= (unsigned long)t->decl.p;
        for (long i = 0; t->b.v && i < t->b.v->n; i++) h = (h ^ type_plain(vec_get(t->b.v, i), effects)) * 1099511628211UL;
        return h;
    case TY_LIST:
    case TY_RESULT:
    case TY_MAP:
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE:
        h = (h ^ type_plain(t->a.t, effects)) * 1099511628211UL;
        return t->kind == TY_LIST || (t->kind == TY_RESULT && !t->b.t) ? h : (h ^ type_plain(t->b.t, effects)) * 31; /* spec 032 */
    default: return h ^ (unsigned long)t->a.i;
    }
}

/* A function's signature hash without effects, and in *effects whether it has any (its `uses` clause, or
 * effects in a parameter or return type). */
static unsigned long sig_plain(Node *fn, int *effects)
{
    Fx *fx = item_fx(fn);
    *effects = fx && fx->declared && !es_is_empty(fx->declared);
    unsigned long h = 1469598103934665603UL;
    for (long i = 0; fn->b.v && i < fn->b.v->n; i++) h = (h ^ type_plain(((Node *)vec_get(fn->b.v, i))->type, effects)) * 1099511628211UL;
    return (h ^ type_plain(fn->type, effects)) * 31;
}

static Vec *effect_only_copies(Vec *fns)
{
    keep_literals = 1;
    Vec *reported = vec_new();
    unsigned long *hashes = alloc((fns->n + 1) * sizeof(unsigned long));
    /* only functions sharing a plain signature with one that has effects can be effect-only copies */
    Vec *with = vec_new();
    unsigned long *sig = alloc((fns->n + 1) * sizeof(unsigned long));
    for (long i = 0; i < fns->n; i++) {
        int effects = 0;
        sig[i] = sig_plain(vec_get(fns, i), &effects);
        if (effects) vec_push(with, (void *)sig[i]);
    }
    for (long i = 0; i < fns->n; i++) {
        Node *fn = vec_get(fns, i);
        int candidate = 0;
        for (long k = 0; k < with->n && !candidate; k++) candidate = (unsigned long)vec_get(with, k) == sig[i];
        hashes[i] = 0;
        if (!candidate) continue;
        anon_locals = 1;
        SB *sb = scratch_start();
        body_form(sb, fn, scratch_names);
        anon_locals = 0;
        unsigned long h = scratch_hash();
        hashes[i] = h ^ (fnv_raw((char *)&fn->b.v->n, sizeof(long)) * 31); /* (parameter counts too) */
        for (long j = 0; j < i; j++) {
            Node *first = vec_get(fns, j);
            if (!hashes[j] || hashes[j] != hashes[i]) continue; /* only equal hashes are compared */
            if (!same_but_effects(first, fn)) continue;
            EffSet *a = item_fx(first)->declared, *b = item_fx(fn)->declared;
            int same = a && b ? es_equal(a, b) : a == b;
            if (same) continue; /* not an effect-only copy (a plain duplicate, or too short to matter) */
            Diag *d = st_report("W0704", fn, (int)fn->file, fn->a.n->start, fn->a.n->end,
                                msg("`$1` repeats `$2` except for its effects: make one function with an effect "
                                    "parameter, `[E: effects]`, and use it for both.",
                                    fn->a.n->a.s, first->a.n->a.s, 0));
            if (d) diag_add_related(d, (int)first->file, first->a.n->start, first->a.n->end, str_from_cstr("the first copy"));
            vec_push(reported, fn);
            break;
        }
    }
    keep_literals = 0;
    return reported;
}

/* W0704 over the program's non-test functions. */
static void rule_duplicates(Vec *files)
{
    dup_wins = vec_new();
    Vec *fns = vec_new();
    for (long k = 0; k < files->n; k++) {
        Vec *items = file_get((int)(long)vec_get(files, k))->ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *it = vec_get(items, i);
            if (it->kind != N_FN) continue;
            vec_push(fns, it);
            block_windows(it->d.n, it);
        }
    }
    Vec *skip = effect_only_copies(fns);
    /* each window: the first earlier equal one (not overlapping it in the same block) */
    long cap = 1;
    while (cap < dup_wins->n * 2 + 2) cap <<= 1;
    Win **table = alloc(cap * sizeof(Win *));
    Win **firsts = alloc((dup_wins->n + 1) * sizeof(Win *));
    for (long i = 0; i < dup_wins->n; i++) {
        Win *w = vec_get(dup_wins, i);
        unsigned long slot = w->hash & (unsigned long)(cap - 1);
        Win *f;
        int same_block = 0;
        while ((f = table[slot])) {
            if (f->hash == w->hash) {
                if (f->stmts == w->stmts) { /* one block repeating itself (`let a = field(x, "a"); let b = ...`) */
                    same_block = 1;          /* is not a copy: copies are in different blocks (research R5) */
                    break;
                }
                if (!f->key) f->key = win_key(f->stmts, f->at);
                if (!w->key) w->key = win_key(w->stmts, w->at);
                if (str_eq(f->key, w->key)) break;
            }
            slot = (slot + 1) & (unsigned long)(cap - 1);
        }
        if (same_block) continue;
        if (!f) {
            table[slot] = w;
            continue;
        }
        firsts[i] = f;
    }
    /* maximal runs: consecutive windows of one block whose firsts are consecutive too */
    for (long i = 0; i < dup_wins->n; i++) {
        if (!firsts[i]) continue;
        Win *w = vec_get(dup_wins, i), *f = firsts[i];
        long j = i;
        while (j + 1 < dup_wins->n && firsts[j + 1]) {
            Win *nw = vec_get(dup_wins, j + 1), *nf = firsts[j + 1];
            if (nw->stmts != w->stmts || nw->at != w->at + (j + 1 - i) || nf->stmts != f->stmts ||
                nf->at != f->at + (j + 1 - i))
                break;
            j++;
        }
        int skipped = 0;
        for (long k = 0; k < skip->n; k++) skipped |= vec_get(skip, k) == w->fn;
        if (!skipped) {
            long n = j - i + D_DUPLICATE;
            Node *a = vec_get(w->stmts, w->at), *z = vec_get(w->stmts, w->at + n - 1);
            Node *fa = vec_get(f->stmts, f->at), *fz = vec_get(f->stmts, f->at + n - 1);
            Diag *d = st_report("W0704", w->fn, (int)a->file, a->start, z->end,
                                msg("These $1 statements repeat $2: make one function and pass what differs as "
                                    "parameters.",
                                    int_to_str(n), where(fa), 0));
            if (d) diag_add_related(d, (int)fa->file, fa->start, fz->end, str_from_cstr("the first copy"));
        }
        i = j;
    }
}

/* ---- W0705 (User Story 4): a test in another file than the code it tests ---- */

/* The files of the program functions that the calls in n (statements, expressions, anonymous functions)
 * reach, added to files once each; *calls counts them. */
static void calls_of(Node *n, Vec *files, long *calls)
{
    if (!n) return;
    switch (n->kind) {
    case N_BLOCK:
        for (long i = 0; i < n->a.v->n; i++) calls_of(vec_get(n->a.v, i), files, calls);
        return;
    case N_CALL:
        if (!n->d.i && n->c.n && own_file((int)n->c.n->file)) {
            long f = n->c.n->file, seen = 0;
            for (long k = 0; k < files->n; k++) seen |= (long)vec_get(files, k) == f;
            if (!seen) vec_push(files, (void *)f);
            (*calls)++;
        }
        for (long i = 0; n->b.v && i < n->b.v->n; i++) calls_of(vec_get(n->b.v, i), files, calls);
        return;
    case N_LAMBDA: calls_of(n->d.n, files, calls); return;
    case N_IF:
        calls_of(n->a.n, files, calls);
        calls_of(n->b.n, files, calls);
        calls_of(n->c.n, files, calls);
        return;
    case N_FOR:
        calls_of(n->b.n, files, calls);
        calls_of(FOR_END(n), files, calls);
        calls_of(n->d.n, files, calls);
        return;
    case N_MATCH:
        calls_of(n->a.n, files, calls);
        for (long i = 0; n->b.v && i < n->b.v->n; i++) calls_of(((Node *)vec_get(n->b.v, i))->c.n, files, calls);
        return;
    case N_LET: calls_of(n->c.n, files, calls); return;
    case N_WHILE:
    case N_SET:
    case N_INDEX:
    case N_BINARY:
        calls_of(n->a.n, files, calls);
        calls_of(n->b.n, files, calls);
        return;
    case N_RETURN:
    case N_EXPR_STMT:
    case N_TRY:
    case N_FIELD:
    case N_UNARY: calls_of(n->a.n, files, calls); return;
    case N_CALL_VALUE:
        calls_of(n->a.n, files, calls);
        for (long i = 0; n->b.v && i < n->b.v->n; i++) calls_of(vec_get(n->b.v, i), files, calls);
        return;
    case N_NEW_RECORD:
        for (long i = 0; n->b.v && i < n->b.v->n; i++) calls_of(vec_get(n->b.v, i), files, calls);
        return;
    case N_LIST:
        for (long i = 0; n->a.v && i < n->a.v->n; i++) calls_of(vec_get(n->a.v, i), files, calls);
        return;
    case N_VCASE: calls_of(n->c.n, files, calls); return;
    case N_FINIT: calls_of(n->b.n, files, calls); return;
    default: return;
    }
}

static void rule_test_placement(Node *test)
{
    Vec *files = vec_new();
    long calls = 0;
    calls_of(test->b.n, files, &calls);
    if (!calls || files->n != 1 || (long)vec_get(files, 0) == (long)test->file) return;
    Str *other = file_get((int)(long)vec_get(files, 0))->path;
    st_report("W0705", test, (int)test->file, test->start, test->start + 4,
              msg("This test calls only functions of `$1`: move it into `$1`, next to the code it tests (`vo test` "
                  "runs it from there).",
                  other, 0, 0));
}

/* ---- after the rules ---- */

/* W0706: every `@allow` in the analyzed files that accepted nothing; the fix removes its line. */
static void unused_allows(Vec *files)
{
    for (long i = 0; allows && i < allows->n; i++) {
        Allow *a = vec_get(allows, i);
        if (a->used) continue;
        int file = (int)a->ann->file, in = 0;
        for (long k = 0; k < files->n; k++) in |= (int)(long)vec_get(files, k) == file;
        if (!in) continue;
        Str *code = ((Node *)vec_get(a->ann->b.v, 0))->a.s;
        Diag *d = diag_add("W0706", SEV_WARNING, file, a->ann->start, a->ann->end,
                           msg("`@allow(\"$1\", ...)` silences nothing here: remove it.", code, 0, 0));
        FileRec *f = file_get(file);
        long from = a->ann->start, to = a->ann->end;
        while (from > 0 && f->text[from - 1] != '\n') from--; /* the whole line, with its newline */
        while (to < f->len && f->text[to] != '\n') to++;
        if (to < f->len) to++;
        diag_add_fix(d, file, from, to, str_from_cstr(""));
        a->used = 1; /* reported once */
    }
}

/* Runs the rules on the program of entry (its own files, each analyzed once). */
void structure_check(int entry, Vec *program)
{
    (void)entry;
    if (env_on("VO_STRUCTURE_OFF=1", 19)) return;
    if (!done) done = vec_new();
    Vec *files = vec_new(); /* the program's own files not analyzed yet */
    for (long i = 0; i < program->n; i++) {
        long id = (long)vec_get(program, i);
        if (!own_file((int)id)) continue;
        int seen = 0;
        for (long k = 0; k < done->n; k++) seen |= (long)vec_get(done, k) == id;
        if (seen) continue;
        vec_push(done, (void *)id);
        vec_push(files, (void *)id);
    }
    for (long k = 0; k < files->n; k++) {
        rule_file_size((int)(long)vec_get(files, k));
        Vec *items = file_get((int)(long)vec_get(files, k))->ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *it = vec_get(items, i);
            if (it->kind != N_FN && it->kind != N_TEST) continue;
            Fx *fx = it->kind == N_FN ? item_fx(it) : 0;
            int effects = fx && fx->declared && !es_is_empty(fx->declared);
            Count c = count_body(it->kind == N_FN ? it->d.n : it->b.n, effects);
            if (it->kind == N_FN) rule_effectful_logic(it, &c);
            rule_function_size(it, &c);
            if (it->kind == N_TEST) rule_test_placement(it);
        }
    }
    rule_duplicates(files);
    unused_allows(files);
}
