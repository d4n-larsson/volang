/* Spec 056: the library's changes since vo 0.1 and E0214 `library_changed` (research R1). One table is the
 * single source of the diagnostic, its fixes, the catalog's list (docs/errors.md E0214) and the changelog.
 * An import of a renamed or removed item reports E0214 at the import and binds the old local name to the
 * new item (a removed item stays in std, private, as its replacement), so the file keeps checking; each
 * use then reports E0214 with the fix of its kind. Rows match std items only. */
#include "vo.h"

enum { LC_RENAME = 1, LC_REMOVE, LC_SWAP, LC_OPTION, LC_FIELD, LC_CASE, LC_ARG };

struct LibChange {
    const char *module; /* std file stem: "lists" */
    const char *old;    /* item, or `Record.field`, or `Variant.case` */
    int kind;
    const char *new_;   /* the new name (rename, field, case; a swap's new name, if renamed) */
    const char *call;   /* remove: what a call becomes; option: the wrap, `$` the call */
    const char *value;  /* remove: what a value use becomes */
    long factor;        /* rename/field: the first argument or the value is multiplied by this (units) */
};

static const LibChange rows[] = {
    /* US1: names that clashed between modules (FR-004) */
    {"lists", "contains", LC_RENAME, "has_item", 0, 0, 0},
    {"json", "field", LC_RENAME, "member", 0, 0, 0},
    {"tcp", "close", LC_RENAME, "close_conn", 0, 0, 0},
    {"tcp", "send", LC_RENAME, "write_all", 0, 0, 0},
    {"tcp", "describe", LC_RENAME, "tcp_error_text", 0, 0, 0},
    {"metrics", "add", LC_RENAME, "add_to", 0, 0, 0},
    {"sqlite", "get", LC_RENAME, "value_column", 0, 0, 0},
    {"sqlite", "get_int", LC_RENAME, "int_column", 0, 0, 0},
    {"sqlite", "get_float", LC_RENAME, "float_column", 0, 0, 0},
    {"sqlite", "get_str", LC_RENAME, "text_column", 0, 0, 0},
    {"sqlite", "get_bytes", LC_RENAME, "bytes_column", 0, 0, 0},
    /* US2: lookups that find nothing give an Option (FR-005) */
    {"http", "header", LC_RENAME, "response_header", 0, 0, 0},
    {"http", "response_header", LC_OPTION, 0, "$header", 0, 0},
    {"http", "request_header", LC_OPTION, 0, "$header", 0, 0},
    {"regex", "first_match", LC_OPTION, 0, "no match", 0, 0},
    {"collections", "first_key", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "last_key", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "key_at_or_above", LC_OPTION, 0, "none", 0, 0},
    {"collections", "key_at_or_below", LC_OPTION, 0, "none", 0, 0},
    {"collections", "key_after", LC_OPTION, 0, "none", 0, 0},
    {"collections", "key_before", LC_OPTION, 0, "none", 0, 0},
    {"collections", "first", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "last", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "at_or_above", LC_OPTION, 0, "none", 0, 0},
    {"collections", "at_or_below", LC_OPTION, 0, "none", 0, 0},
    {"collections", "after", LC_OPTION, 0, "none", 0, 0},
    {"collections", "before", LC_OPTION, 0, "none", 0, 0},
    {"collections", "peek", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "front", LC_OPTION, 0, "empty", 0, 0},
    {"collections", "back", LC_OPTION, 0, "empty", 0, 0},
    /* US5: the text first, as text and the builtins (FR-010) */
    {"regex", "replace_all", LC_RENAME, "replace_matches", 0, 0, 0},
    {"regex", "is_match", LC_SWAP, 0, 0, 0, 0},
    {"regex", "first_match", LC_SWAP, 0, 0, 0, 0},
    {"regex", "all_matches", LC_SWAP, 0, 0, 0, 0},
    {"regex", "split_on", LC_SWAP, 0, 0, 0, 0},
    {"regex", "replace_matches", LC_SWAP, 0, 0, 0, 0},
    /* US5: one name for an expired time limit (FR-008) */
    {"workers", "WorkerError.timeout", LC_CASE, "timed_out", 0, 0, 0},
    /* US5: functions take milliseconds, and a duration's name says its unit (FR-009, research R7) */
    /* US5: one name for one thing (FR-012 to FR-014) */
    {"http", "serve_bytes_workers", LC_RENAME, "serve_workers_bytes", 0, 0, 0},
    {"random", "shuffle", LC_RENAME, "shuffled", 0, 0, 0},
    {"web", "clock_ms", LC_REMOVE, 0, "now_ms()", "fn() -> int { return now_ms(); }", 0},
    {"time", "now", LC_REMOVE, 0, "now_ms()", "fn() -> int { return now_ms(); }", 0},
    {"time", "fixed_zone", LC_RENAME, "offset_zone", 0, 0, 60000},
    {"web", "Cookie.max_age_s", LC_FIELD, "max_age_ms", 0, 0, 1000},
    {"security", "Browser.lifetime_s", LC_FIELD, "lifetime_ms", 0, 0, 1000},
    /* spec 057: a working directory after the variables; "./" keeps the old behavior (arg: the
       inserted text in `call`, the note in `value`, its index in `factor`) */
    {"process", "run_with", LC_ARG, "a working directory after the variables", "\"./\"",
     "`\"./\"` is the start directory", 4},
    {"process", "run_with_bytes", LC_ARG, "a working directory after the variables", "\"./\"",
     "`\"./\"` is the start directory", 4},
    {"process", "run_lines", LC_ARG, "a working directory after the variables", "\"./\"",
     "`\"./\"` is the start directory", 4},
    {"time", "Zone.starts", LC_FIELD, "starts_s", 0, 0, 0},
    {"time", "Zone.offsets", LC_FIELD, "offsets_s", 0, 0, 0},
    {"time", "ZoneRule.std_offset", LC_FIELD, "std_offset_s", 0, 0, 0},
    {"time", "ZoneRule.dst_offset", LC_FIELD, "dst_offset_s", 0, 0, 0},
    {"time", "RuleDay.seconds", LC_FIELD, "time_s", 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

static int cstr_is(Str *s, const char *c)
{
    return c && str_eq_cstr(s, (char *)c);
}

/* The row of module.old of this kind (0: any kind). */
static const LibChange *find_kind(Str *module, Str *old, int kind)
{
    for (const LibChange *c = rows; c->module; c++)
        if (cstr_is(module, c->module) && cstr_is(old, c->old) && (!kind || c->kind == kind)) return c;
    return 0;
}

const LibChange *libchange_find(Str *module, Str *old)
{
    const LibChange *c = find_kind(module, old, LC_RENAME);
    return c ? c : find_kind(module, old, LC_REMOVE);
}

Str *libchange_new_name(const LibChange *c)
{
    return c->new_ ? str_from_cstr((char *)c->new_) : 0;
}

static Str *qualified(const LibChange *c)
{
    SB *sb = sb_new();
    sb_cstr(sb, "std.");
    sb_cstr(sb, (char *)c->module);
    sb_byte(sb, '.');
    sb_cstr(sb, (char *)c->old);
    return sb_finish(sb);
}

static Str *message(const LibChange *c)
{
    Str *q = qualified(c);
    switch (c->kind) {
    case LC_REMOVE: return msg("`$1` was removed in vo 0.2.0: write `$2`.", q, str_from_cstr((char *)c->call), 0);
    case LC_SWAP: return msg("`$1` takes the text first since vo 0.2.0.", q, 0, 0);
    case LC_OPTION: return msg("`$1` returns an `Option` since vo 0.2.0: `some(value)` or `none`.", q, 0, 0);
    case LC_CASE: return msg("`$1` was renamed `$2` in vo 0.2.0.", q, str_from_cstr((char *)c->new_), 0);
    case LC_ARG: /* spec 057 */
        return msg("`$1` takes $2 since vo 0.3.0: $3.", q, str_from_cstr((char *)c->new_), str_from_cstr((char *)c->value));
    case LC_FIELD:
        if (c->factor) return msg("`$1` was renamed `$2` in vo 0.2.0, and holds milliseconds.", q, str_from_cstr((char *)c->new_), 0);
        return msg("`$1` was renamed `$2` in vo 0.2.0.", q, str_from_cstr((char *)c->new_), 0);
    default:
        if (c->factor) return msg("`$1` was renamed `$2` in vo 0.2.0, and takes milliseconds.", q, str_from_cstr((char *)c->new_), 0);
        return msg("`$1` was renamed `$2` in vo 0.2.0.", q, str_from_cstr((char *)c->new_), 0);
    }
}

/* At the import of a changed item: E0214; the fix gives the new name (a removed item: the import goes). */
void libchange_import(Node *name_node, Str *module, const LibChange *c)
{
    (void)module;
    Diag *d = ck_err("E0214", name_node, message(c));
    if (c->kind == LC_RENAME) {
        diag_add_fix(d, (int)name_node->file, name_node->start, name_node->end, str_from_cstr((char *)c->new_));
    } else if (c->kind == LC_REMOVE) { /* the whole `import ...;` line */
        FileRec *f = file_get((int)name_node->file);
        long s = name_node->start, e = name_node->end;
        while (s > 0 && f->text[s - 1] != '\n') s--;
        while (e < f->len && f->text[e] != '\n') e++;
        if (e < f->len) e++;
        diag_add_fix(d, (int)name_node->file, s, e, str_from_cstr(""));
    }
}

/* The source text of a node. */
static Str *text_of(Node *n)
{
    FileRec *f = file_get((int)n->file);
    return str_new(f->text + n->start, n->end - n->start);
}

/* e in the new unit: a literal multiplied out, a name or call times the factor, else (e) times it. */
static void scaled(SB *sb, Node *e, long factor)
{
    if (e->kind == N_INT) {
        sb_str(sb, int_to_str(e->a.i * factor));
        return;
    }
    int bare = e->kind == N_IDENT || e->kind == N_CALL || e->kind == N_FIELD;
    if (!bare) sb_byte(sb, '(');
    sb_str(sb, text_of(e));
    if (!bare) sb_byte(sb, ')');
    sb_cstr(sb, " * ");
    sb_str(sb, int_to_str(factor));
}

/* A use of the old name: E0214 with the fix of its kind. call: the call whose callee this is, or 0. */
void libchange_use(Sym *sym, Node *node, Node *call)
{
    const LibChange *c = sym->change;
    if (!c || (c->kind != LC_RENAME && c->kind != LC_REMOVE)) return;
    Diag *d = ck_err("E0214", node, message(c));
    int file = (int)node->file;
    if (c->kind == LC_REMOVE) {
        if (call) diag_add_fix(d, file, call->start, call->end, str_from_cstr((char *)c->call));
        else if (c->value) diag_add_fix(d, file, node->start, node->end, str_from_cstr((char *)c->value));
        return;
    }
    Node *arg0 = call && call->b.v && call->b.v->n ? vec_get(call->b.v, 0) : 0;
    if (!c->factor || !arg0) { /* (a unit change on a value use: the name only; the message says the unit) */
        diag_add_fix(d, file, node->start, node->end, str_from_cstr((char *)c->new_));
        return;
    }
    SB *sb = sb_new(); /* NEW( then the first argument in the new unit */
    sb_cstr(sb, (char *)c->new_);
    FileRec *f = file_get(file);
    sb_bytes(sb, f->text + node->end, arg0->start - node->end);
    scaled(sb, arg0, c->factor);
    diag_add_fix(d, file, node->start, arg0->end, sb_finish(sb));
}

int libchange_binds(const LibChange *c)
{
    return c && (c->kind == LC_RENAME || c->kind == LC_REMOVE);
}

/* The row of this kind of a std function declaration, or 0. */
static const LibChange *row_of_decl(Node *decl, int kind)
{
    if (!decl || decl->kind != N_FN || !decl->a.n) return 0;
    FileRec *f = file_get((int)decl->file);
    if (!f->std) return 0;
    return find_kind(str_new(f->path->data + 4, f->path->len - 7), decl->a.n->a.s, kind);
}

Node *ck_result_ctx; /* the expression a `match` on ok/err, a `try` or a comparison with ok/err reads as a result */

/* A call of a lookup that now gives an Option, read as the result it gave before (an expected `result`
 * type, or the node ck_result_ctx): E0214, with a fix wrapping the call in `to_result(CALL, OLD_ERROR)` (and
 * the import, when missing); the call then checks as that result. Otherwise t unchanged. */
Type *libchange_option_type(Node *call, Type *t, Type *expected)
{
    if (!call || call->kind != N_CALL || ty_bad(t) || t->kind != TY_VARIANT || !t->b.v || t->b.v->n != 1) return t;
    const LibChange *c = row_of_decl(call->c.n, LC_OPTION);
    if (!c) return t;
    if (!(expected && expected->kind == TY_RESULT) && call != ck_result_ctx) return t;
    Diag *d = ck_err("E0214", call, message(c));
    int file = (int)call->file;
    FileRec *f = file_get(file);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    long from = call->start;
    if (!scope_find(str_from_cstr("to_result"))) { /* one edit from the imports to the call (a fix is one edit) */
        from = import_insert_at(file);
        sb_cstr(sb, "import std.option.to_result;\n");
        sb_bytes(sb, f->text + from, call->start - from);
    }
    sb_cstr(sb, "to_result(");
    sb_bytes(sb, f->text + call->start, call->end - call->start);
    Node *name_arg = call->b.v && call->b.v->n > 1 ? vec_get(call->b.v, 1) : 0;
    if (str_eq_cstr(str_from_cstr((char *)c->call), "$header") && name_arg) { /* the old text: no header `NAME` */
        sb_cstr(sb, ", \"no header `\" + ");
        sb_bytes(sb, f->text + name_arg->start, name_arg->end - name_arg->start);
        sb_cstr(sb, " + \"`\")");
    } else { /* the old error text */
        sb_cstr(sb, ", \"");
        sb_cstr(sb, (char *)c->call);
        sb_cstr(sb, "\")");
    }
    diag_add_fix(d, file, from, call->end, sb_finish(sb));
    GC_UNROOT(1);
    return ty_result((Type *)t->b.v->data[0].p);
}

/* A call of a std function that now takes the text first (non-generic): checks its arguments. In the old
 * order (the first argument has the second parameter's type), E0214 with a fix swapping the first two.
 * 1 when the call's arguments were checked here, 0 for any other call. */
int libchange_swap_call(Node *call)
{
    Node *decl = call->c.n;
    if (!row_of_decl(decl, LC_SWAP) || !call->b.v || call->b.v->n < 2 || decl->b.v->n < 2) return 0;
    if (call->b.v->n != decl->b.v->n) return 0; /* (the usual E0302) */
    Type *p0 = ((Node *)vec_get(decl->b.v, 0))->type, *p1 = ((Node *)vec_get(decl->b.v, 1))->type;
    Node *a0 = vec_get(call->b.v, 0), *a1 = vec_get(call->b.v, 1);
    Type *t0 = check_expr(a0, 0);
    if (!ty_bad(t0) && t0 == p1 && p0 != p1) { /* the old order */
        Diag *d = ck_err("E0214", call->a.n, message(row_of_decl(decl, LC_SWAP)));
        FileRec *f = file_get((int)call->file);
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_bytes(sb, f->text + a1->start, a1->end - a1->start);
        sb_bytes(sb, f->text + a0->end, a1->start - a0->end);
        sb_bytes(sb, f->text + a0->start, a0->end - a0->start);
        diag_add_fix(d, (int)call->file, a0->start, a1->end, sb_finish(sb));
        GC_UNROOT(1);
        expect_type(a1, p0, check_expr(a1, p0));
    } else {
        expect_type(a0, p0, t0);
        expect_type(a1, p1, check_expr(a1, p1));
    }
    for (long i = 2; i < call->b.v->n; i++) {
        Node *a = vec_get(call->b.v, i);
        Type *p = ((Node *)vec_get(decl->b.v, i))->type;
        expect_type(a, p, check_expr(a, p));
    }
    return 1;
}

/* An unknown case name of a std variant: when the case was renamed, E0214 with the new name as the fix,
 * and the new case's index; else -1 (the caller reports the unknown case). */
long libchange_case(Node *decl, Node *case_name)
{
    if (!decl || !case_name) return -1;
    FileRec *f = file_get((int)decl->file);
    if (!f->std) return -1;
    SB *sb = sb_new();
    sb_str(sb, decl->a.n->a.s);
    sb_byte(sb, '.');
    sb_str(sb, case_name->a.s);
    const LibChange *c = find_kind(str_new(f->path->data + 4, f->path->len - 7), sb_finish(sb), LC_CASE);
    if (!c) return -1;
    long idx = member_index(decl->b.v, str_from_cstr((char *)c->new_));
    if (idx < 0) return -1;
    Diag *d = ck_err("E0214", case_name, message(c));
    diag_add_fix(d, (int)case_name->file, case_name->start, case_name->end, str_from_cstr((char *)c->new_));
    return idx;
}

/* An unknown field of a std record: when the field was renamed, E0214 with a fix, and the new field's
 * index; else -1. name: the field name in the source; whole: a read `X.old` (value 0); value: the value a
 * literal gives or a `set` stores, converted with the row's factor. A read of a field now in milliseconds
 * becomes `(X.new / factor)`, so the program's arithmetic keeps its unit. */
long libchange_field(Node *decl, Node *name, Node *whole, Node *value)
{
    if (!decl || !name) return -1;
    FileRec *df = file_get((int)decl->file);
    if (!df->std) return -1;
    SB *sb = sb_new();
    sb_str(sb, decl->a.n->a.s);
    sb_byte(sb, '.');
    sb_str(sb, name->a.s);
    const LibChange *c = find_kind(str_new(df->path->data + 4, df->path->len - 7), sb_finish(sb), LC_FIELD);
    if (!c) return -1;
    long idx = member_index(decl->b.v, str_from_cstr((char *)c->new_));
    if (idx < 0) return -1;
    Diag *d = ck_err("E0214", name, message(c));
    int file = (int)name->file;
    FileRec *f = file_get(file);
    Str *nn = str_from_cstr((char *)c->new_);
    if (!c->factor) {
        diag_add_fix(d, file, name->start, name->end, nn);
        return idx;
    }
    sb = sb_new();
    GC_ROOT(&sb);
    if (!value) { /* a read: (X.new / factor) */
        sb_byte(sb, '(');
        sb_bytes(sb, f->text + whole->start, name->start - whole->start);
        sb_str(sb, nn);
        sb_cstr(sb, " / ");
        sb_str(sb, int_to_str(c->factor));
        sb_byte(sb, ')');
        diag_add_fix(d, file, whole->start, whole->end, sb_finish(sb));
    } else { /* new: value, or set X.new = value, in the new unit */
        sb_str(sb, nn);
        sb_bytes(sb, f->text + name->end, value->start - name->end);
        scaled(sb, value, c->factor);
        diag_add_fix(d, file, name->start, value->end, sb_finish(sb));
    }
    GC_UNROOT(1);
    return idx;
}

/* Spec 057: a call of a std function that gained an argument (LC_ARG), written with one argument fewer:
 * E0214 with a fix inserting it after argument factor - 1, and the argument is inserted into the call so
 * it checks as written with it (the inserted value keeps the old behavior). Before fx_call, which reads
 * the arguments' literal targets. */
void libchange_arg_call(Node *call)
{
    Node *decl = call->c.n;
    const LibChange *c = row_of_decl(decl, LC_ARG);
    if (!c || !call->b.v || call->b.v->n != decl->b.v->n - 1 || call->b.v->n < c->factor) return;
    Node *before = vec_get(call->b.v, c->factor - 1);
    Diag *d = ck_err("E0214", call->a.n, message(c));
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, ", ");
    sb_cstr(sb, (char *)c->call);
    diag_add_fix(d, (int)call->file, before->end, before->end, sb_finish(sb));
    GC_UNROOT(1);
    long n = cstr_len((char *)c->call);
    Node *arg = node_new(N_STR, before->end, before->end); /* the literal's value: `call` without its quotes */
    arg->file = call->file;
    GC_ROOT(&arg);
    arg->a.s = str_new((char *)c->call + 1, n - 2);
    vec_push(call->b.v, arg);
    GC_UNROOT(1);
    for (long i = call->b.v->n - 1; i > c->factor; i--) call->b.v->data[i] = call->b.v->data[i - 1];
    call->b.v->data[c->factor].p = arg;
}
