/* Queries on a checked program (spec 006 US1): `vo query callers|refs|type-at` and `vo impact`
 * (ported from src/query.s, spec 008). The checker logs every resolved name while q_collect is 1
 * (research R1); this file resolves targets (R2), answers from the log (R3, R4) and reads unified
 * diffs for impact (R5). */
#include "vo.h"

long q_collect;      /* 1 while query/impact/edit check a program */
Vec *q_log;          /* RefLog entries */
Node *ck_item;       /* the function/test/property being checked, or 0 */
Vec *q_exprs;        /* every expression node checked while q_collect is 1 (type-at) */
static Vec *df_plus;    /* vo impact: keys file << 32 | line of added lines */
static Vec *df_minus;   /* places (file << 32 | new-side line) where lines were removed */
static Vec *df_removed; /* {file, name str} of removed `fn NAME(` lines */
static Vec *df_ignored; /* diff paths that are not program files (strs) */
static Vec *im_list;    /* impact entries */

/* An impact entry: an item (or a removed or ignored name, a str), its role, and what led to it. */
typedef struct Impact {
    void *x;
    char *role;
    void *via;
} Impact;
CHECK_SIZE(Impact, 24);

/* Impact roles are compared by address. */
static char r_changed[] = "changed";
static char r_added[] = "added";
static char r_removed[] = "removed";
static char r_affected[] = "affected";
static char r_test[] = "test";
static char r_ignored[] = "ignored";

static char *const role_names[] = {"declaration", "call", "value", "type", "import", "use"};

#define QK_ITEMS ((1L << N_FN) | (1L << N_RECORD) | (1L << N_VARIANT) | (1L << N_EFFECTS) | (1L << N_CONST))
#define QK_TYPES ((1L << N_RECORD) | (1L << N_VARIANT))
#define M_DF_HUNK "The diff is not a valid unified diff (a hunk is malformed or cut short)."
#define J_ROLE ",\"role\":\""
#define J_CLOSE "}\n"

/* Sets ck_item to item when it is a function, test or property (else 0); returns item. */
Node *ck_item_set(Node *item)
{
    ck_item = item && (item->kind == N_FN || item->kind == N_TEST) ? item : 0;
    return item;
}

/* Appends a reference to the log while q_collect is 1 (research R1); the entry, or 0. Nodes without a
 * span (synthetic ones, such as `ret` in contracts) are not references. */
RefLog *q_note(long key_a, long key_b, Node *node, long role)
{
    if (!q_collect || !node || node->start >= node->end) return 0;
    RefLog *r = alloc(RL_SIZE);
    r->key_a.i = key_a;
    r->key_b.i = key_b;
    r->node = node;
    r->role = role;
    r->item = ck_item;
    if (!q_log) q_log = vec_new();
    vec_push(q_log, r);
    return r;
}

/* A reference to a symbol: items by their declaration, locals and type parameters by their
 * declaring name node. Placeholders (undefined names) are skipped. */
void q_note_sym(Sym *sym, Node *node, long role)
{
    if (!q_collect || !sym->node) return;
    long key = (long)sym->node;
    if (sym->kind != SYM_LOCAL && sym->kind != SYM_TPARAM) {
        if (!sym->decl) return;
        key = (long)sym->decl;
    }
    q_note(key, -1, node, role);
}

/* The declaration of a new symbol: an item's own name, an import's copy (role import), or a
 * parameter, local or type parameter. */
void q_note_decl(Sym *sym)
{
    if (!q_collect) return;
    Node *nn = sym->node;
    if (!nn) return;
    long role = RR_DECL;
    long key = (long)nn;
    if (sym->kind != SYM_LOCAL && sym->kind != SYM_TPARAM) {
        Node *decl = sym->decl;
        if (!decl) return;
        if (decl->a.n != nn) role = RR_IMPORT;
        key = (long)decl;
    }
    RefLog *r = q_note(key, -1, nn, role);
    if (r) r->type = sym->type;
}

/* ---- loading a program for queries ---- */

/* Loads and checks the program with the reference log on, adds the declarations of record fields
 * and variant cases, and returns the entry's file id. A syntax error that stops checking prints
 * the diagnostics and exits 65 (research R6). */
long q_load(char *entry)
{
    long file = (long)load_source(str_from_cstr(entry));
    doc_entry_file = file; /* spec 023: module names of the program's files, for doc records */
    q_collect = 1;
    check_program((int)file, 0);
    if (ld_hard) vo_exit(finish_check(1));
    q_members_all();
    q_collect = 0;
    return file;
}

/* Declaration entries for the fields and cases of every record and variant of the program. */
void q_members_all(void)
{
    ck_item = 0;
    Vec *files = ld_files();
    for (long i = 0; i < files->n; i++) {
        Vec *items = file_get((int)(long)vec_get(files, i))->ast->a.v;
        for (long j = 0; j < items->n; j++) {
            Node *it = vec_get(items, j);
            if (it->kind == N_RECORD || it->kind == N_VARIANT) q_members(it);
        }
    }
}

/* Declaration entries (decl, index) for each field or case, with its type (0 for a case
 * without a value). */
void q_members(Node *decl)
{
    for (long i = 0;; i++) {
        Node *m = member_at(decl->b.v, i);
        if (!m) return;
        RefLog *r = q_note((long)decl, i, m->a.n, RR_DECL);
        if (r && m->b.n) r->type = m->b.n->type;
        if (m->kind == N_CASE_DECL && m->c.n) q_members(m->c.n); /* spec 060: a case's fields */
    }
}

/* keys of `member` of the record or variant d: a field or case, or (spec 060) `case.field`, a field of a
 * case with fields (keyed by its hidden record). target is the whole text, for messages. */
static void q_member_keys(Node *d, char *member, char *target, long *keys)
{
    long dot = cstr_index(member, '.');
    Str *name = dot < 0 ? str_from_cstr(member) : str_new(member, dot);
    long idx = member_index(d->b.v, name);
    if (idx >= 0 && dot >= 0) { /* Variant.case.field */
        Node *m = member_at(d->b.v, idx);
        d = m->kind == N_CASE_DECL ? m->c.n : 0;
        name = str_from_cstr(member + dot + 1);
        idx = d ? member_index(d->b.v, name) : -1;
    }
    if (idx < 0) q_unknown("No field or case `$1` in this program.", str_from_cstr(target), d ? q_suggest(name, 0, d->b.v) : 0);
    keys[0] = (long)d;
    keys[1] = idx;
}

/* ---- targets (research R2) ---- */

/* T0004 unknown_target, exit 64. */
void q_fail(Str *message)
{
    q_fail_code(message, "T0004");
}

void q_fail_code(Str *message, char *code)
{
    toolchain_error((int)opt_json, (int)report_fd, code, message, 0);
    vo_exit(EXIT_USAGE);
}

/* The id of the program file with this path: equal, or one path a suffix of the other at a `/`.
 * -1 when none matches. */
long q_program_file(Str *path)
{
    Vec *files = ld_files();
    for (long i = 0; i < files->n; i++) {
        long id = (long)vec_get(files, i);
        if (path_suffix_match(file_get((int)id)->path, path)) return id;
    }
    return -1;
}

/* 1 when the strs are equal, or the shorter is a suffix of the longer that starts after a `/`.
 * ponytail: paths are matched by text (no realpath); `./a.vo` and `a.vo` match through the suffix rule. */
int path_suffix_match(Str *a, Str *b)
{
    if (a->len < b->len) { /* make a the longer one */
        Str *t = a;
        a = b;
        b = t;
    }
    long off = a->len - b->len; /* where b would start in a */
    for (long i = 0; i < b->len; i++)
        if (a->data[off + i] != b->data[i]) return 0;
    if (!off) return 1; /* equal lengths: equal */
    if (!b->len) return 0;
    return a->data[off - 1] == '/';
}

/* For FILE:LINE:COL, the byte offset (and the file id in *file_out); -1 when text has another
 * form. A file outside the program, or a position outside its file, is T0004. */
long q_parse_pos(char *text, long *file_out)
{
    long before;
    long col = q_tail_number(text, cstr_len(text), &before); /* COL: digits after the last ':' */
    if (col < 0) return -1;
    long line = q_tail_number(text, before, &before);
    if (line < 0 || !before) return -1;
    Str *path = str_new(text, before);
    long file = q_program_file(path);
    if (file < 0)
        q_fail(msg("No file `$1` in this program (a program is its entry file and the files it imports).", path, 0,
                   0));
    *file_out = file;
    long off = pos_offset((int)file, line, col);
    if (off >= 0) return off;
    q_fail(msg("`$1` is outside its file.", str_from_cstr(text), 0, 0));
    return -1;
}

/* The decimal number after the last ':' in text[0..n); -1 when there is none. *before gets the
 * length of the text before that ':'. */
long q_tail_number(char *text, long n, long *before)
{
    long i = n; /* scan back over digits */
    for (;;) {
        if (!i) return -1;
        int c = (unsigned char)text[i - 1];
        if (c < '0' || c > '9') break;
        i--;
    }
    if (i == n || text[i - 1] != ':') return -1; /* no digits, or no ':' */
    long v = 0;
    for (long k = i; k < n; k++) {
        v = v * 10 + (text[k] - '0');
        if (v > 1000000000) return -1;
    }
    *before = i - 1;
    return v;
}

/* The innermost logged reference or declaration whose span contains off, or 0. */
RefLog *q_entry_at(long file, long off)
{
    RefLog *best = 0;
    unsigned long best_len = (unsigned long)-1;
    if (!q_log) return 0;
    for (long i = 0; i < q_log->n; i++) {
        RefLog *r = q_log->data[i].p;
        Node *n = r->node;
        if (n->file != file || n->start > off || off >= n->end) continue;
        unsigned long len = (unsigned long)n->end - n->start;
        if (len >= best_len) continue;
        best_len = len;
        best = r;
    }
    return best;
}

/* An item declaration logged for this name node's own item, of a kind in kinds. */
static Node *q_item_decl(RefLog *r, long kinds)
{
    if (r->role != RR_DECL || r->key_b.i != -1) return 0;
    Node *d = r->key_a.n;
    if (d->a.n != r->node) return 0; /* an item: its own name node */
    if (!((1L << d->kind) & kinds)) return 0;
    return d;
}

/* Every top-level item declaration in the log with this name, of a kind in kinds (bit 1 << N_KIND).
 * Exits with T0005 when several match. */
Node *q_item_named(Str *name, long kinds)
{
    Node *first = 0;
    Vec *all = 0; /* all matches (when more than one) */
    for (long i = 0; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        Node *d = q_item_decl(r, kinds);
        if (!d || !str_eq(r->node->a.s, name) || d == first) continue;
        if (!first) {
            first = d;
            continue;
        }
        if (!all) {
            all = vec_new();
            vec_push(all, first);
        }
        vec_push(all, d);
    }
    if (all) q_ambiguous(name, all); /* several: T0005 with each declaration in related */
    return first;
}

void q_ambiguous(Str *name, Vec *decls)
{
    Str *m = msg("`$1` is declared in several files; name one with FILE:LINE:COL.", name, 0, 0);
    te_related = decls;
    q_fail_code(m, "T0005");
}

/* The closest item name (members == 0) or member name to name within edit distance 2, or 0. */
Str *q_suggest(Str *name, long kinds, Vec *members)
{
    Str *best = 0;
    long dist = 3;
    if (members) {
        for (long i = 0;; i++) {
            Node *m = member_at(members, i);
            if (!m) break;
            Str *cand = m->a.n->a.s;
            long d = name_distance(cand, name);
            if (!d || d >= dist) continue; /* the same name is no suggestion */
            dist = d;
            best = cand;
        }
        return best;
    }
    for (long i = 0; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        if (!q_item_decl(r, kinds)) continue;
        Str *cand = r->node->a.s;
        long d = name_distance(cand, name);
        if (!d || d >= dist) continue;
        dist = d;
        best = cand;
    }
    return best;
}

/* T0004 with "Did you mean `x`?" when there is a suggestion. */
void q_unknown(char *tmpl, Str *name, Str *suggestion)
{
    Str *m = msg(tmpl, name, 0, 0);
    if (suggestion) m = msg("$1 Did you mean `$2`?", m, suggestion, 0);
    q_fail(m);
}

/* The target of a query or edit (research R2): name, Record.field, Variant.case or FILE:LINE:COL.
 * keys[0], keys[1] get its (key_a, key_b). */
void q_resolve(char *target, long *keys)
{
    long file;
    long off = q_parse_pos(target, &file);
    long ka = 0, kb = 0;
    if (off >= 0) { /* a position: the innermost entry there */
        RefLog *r = q_entry_at(file, off);
        if (!r) q_fail(msg("Nothing to look up at `$1`: point at a name.", str_from_cstr(target), 0, 0));
        ka = r->key_a.i;
        kb = r->key_b.i;
    } else if (target[0] >= 'a' && target[0] <= 'z' && cstr_index(target, '.') > 0) {
        q_resolve_qualified(target, keys); /* spec 058: module.name, module.Type.member */
        return;
    } else {
        long dot = cstr_index(target, '.');
        if (dot >= 0) { /* Name.member */
            Str *type_name = str_new(target, dot);
            Node *d = q_item_named(type_name, QK_TYPES);
            if (!d)
                q_unknown("No record or variant named `$1` in this program.", type_name,
                          q_suggest(type_name, QK_TYPES, 0));
            q_member_keys(d, target + dot + 1, target, keys);
            return;
        } else { /* a top-level name, or a builtin */
            Str *name = str_from_cstr(target);
            Node *d = q_item_named(name, QK_ITEMS);
            if (d) {
                ka = (long)d;
                kb = -1;
            } else {
                long id = builtin_id(name);
                if (id)
                    kb = id;
                else
                    q_unknown("Nothing named `$1` in this program.", name, q_suggest(name, QK_ITEMS, 0));
            }
        }
    }
    keys[0] = ka;
    keys[1] = kb;
}

/* Spec 058: a declaration of the module's file by name and kind mask, or 0. */
static Node *q_item_in_file(long file, Str *name, long kinds)
{
    Vec *items = file_get((int)file)->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (!((1L << it->kind) & kinds) || !it->a.n || !str_eq(it->a.n->a.s, name)) continue;
        return it;
    }
    return 0;
}

/* Spec 058 (research R8): `module.name` or `module.Type.member`, the module being a name an import of
 * the program binds (`notes` for `import notes;`). Two files bound under one name: T0005. */
void q_resolve_qualified(char *target, long *keys)
{
    long dot = cstr_index(target, '.');
    Str *mod = str_new(target, dot);
    char *rest = target + dot + 1;
    long found = -1;
    Vec *files = vec_new();
    Vec *order = ld_files();
    for (long i = 0; order && i < order->n; i++) {
        FileRec *f = file_get((int)(long)vec_get(order, i));
        for (Sym *s = f->scope; s; s = s->next)
            if (s->kind == SYM_MODULE && str_eq(s->name, mod) && s->slot != found) {
                int seen = 0;
                for (long k = 0; k < files->n; k++) seen |= (long)vec_get(files, k) == s->slot;
                if (!seen) vec_push(files, (void *)s->slot);
                found = s->slot;
            }
    }
    if (!files->n) q_unknown("No module named `$1` is imported in this program.", mod, 0);
    if (files->n > 1) {
        Vec *decls = vec_new();
        for (long k = 0; k < files->n; k++) vec_push(decls, file_get((int)(long)vec_get(files, k))->ast);
        q_ambiguous(mod, decls);
    }
    long dot2 = cstr_index(rest, '.');
    if (dot2 < 0) {
        Node *d = q_item_in_file(found, str_from_cstr(rest), QK_ITEMS);
        if (!d) q_unknown("No item `$1` in this program.", str_from_cstr(target), 0);
        keys[0] = (long)d;
        keys[1] = -1;
        return;
    }
    Str *type_name = str_new(rest, dot2);
    Node *d = q_item_in_file(found, type_name, QK_TYPES);
    if (!d) q_unknown("No record or variant named `$1` in this program.", str_new(target, dot + 1 + dot2), 0);
    q_member_keys(d, rest + dot2 + 1, target, keys);
}

/* Index of the first c in s, or -1. */
long cstr_index(char *s, int c)
{
    for (char *p = s; *p; p++)
        if ((unsigned char)*p == c) return p - s;
    return -1;
}

/* ---- answers ---- */

/* The log entries of a target, without duplicates (same span), sorted by file and position,
 * the declaration first. */
Vec *q_matches(long key_a, long key_b, long calls_only)
{
    Vec *out = vec_new();
    for (long i = 0; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        if (!q_same_key(key_a, key_b, r->key_a.i, r->key_b.i)) continue;
        if (calls_only && r->role != RR_CALL && r->role != RR_VALUE) continue;
        q_insert_sorted(out, r);
    }
    return out;
}

/* (key_a, key_b) pairs equal; an unresolved name (key_a 0, key_b a str) compares by text. */
int q_same_key(long a1, long a2, long b1, long b2)
{
    if (a1 != b1) return 0;
    if (a2 == b2) return 1;
    if (a1) return 0;
    if ((unsigned long)a2 < 4096 || (unsigned long)b2 < 4096) return 0; /* builtin ids are small */
    return str_eq((Str *)a2, (Str *)b2);
}

/* Inserts by (file, start), declarations before other roles at the same place; drops an entry
 * whose span is already there. */
void q_insert_sorted(Vec *v, RefLog *entry)
{
    Node *n = entry->node;
    unsigned long key = (unsigned long)n->file << 32 | n->start;
    long at = 0;
    for (; at < v->n; at++) {
        Node *m = ((RefLog *)v->data[at].p)->node;
        unsigned long k = (unsigned long)m->file << 32 | m->start;
        if (key < k) break;
        if (key == k && m->end == n->end) return; /* same span: a duplicate */
    }
    vec_push(v, entry); /* append, then shift into place */
    for (long j = v->n - 1; j > at; j--) v->data[j] = v->data[j - 1];
    v->data[at].p = entry;
}

/* Moves the first declaration entry to the front. */
void q_decl_first(Vec *v)
{
    for (long i = 0; i < v->n; i++) {
        RefLog *r = v->data[i].p;
        if (r->role != RR_DECL) continue;
        for (; i > 0; i--) v->data[i] = v->data[i - 1];
        v->data[0].p = r;
        return;
    }
}

/* The source line containing off, without leading and trailing spaces. */
void q_line_text(SB *sb, long file, long off)
{
    long line = pos_line((int)file, off);
    long s = line_start((int)file, line);
    long e = line_end((int)file, line);
    char *text = file_get((int)file)->text;
    char *p = text + s, *q = text + e;
    while (p < q && *p == ' ') p++;
    while (q > p && (q[-1] == ' ' || q[-1] == '\r')) q--;
    sb_bytes(sb, p, q - p);
}

/* Writes the builder to the report fd. */
void q_out(SB *sb)
{
    out_bytes((int)report_fd, sb->ptr, sb->len);
}

/* One reference: a `ref` record, or `FILE:LINE:COL ROLE in ITEM: LINE`. */
void q_ref(RefLog *r, char *target)
{
    Node *n = r->node;
    int file = (int)n->file;
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"ref\",\"target\":");
        json_str_into(sb, str_from_cstr(target));
        sb_cstr(sb, J_ROLE);
        sb_cstr(sb, q_role_name(r->role));
        sb_cstr(sb, "\",\"file\":");
        json_span(sb, file, n->start, n->end);
        sb_cstr(sb, ",\"text\":");
        json_src_text(sb, file, n->start, n->end);
        if (r->item) {
            sb_cstr(sb, ",\"in\":");
            json_str_into(sb, fx_item_name(r->item));
        }
        sb_cstr(sb, J_CLOSE);
    } else {
        human_loc(sb, file, n->start);
        sb_byte(sb, ' ');
        sb_cstr(sb, q_role_name(r->role));
        if (r->item) {
            sb_cstr(sb, " in ");
            sb_str(sb, fx_item_name(r->item));
        }
        sb_cstr(sb, ": ");
        q_line_text(sb, file, n->start);
        sb_byte(sb, '\n');
    }
    q_out(sb);
}

char *q_role_name(long role)
{
    return role_names[role];
}

/* The closing lines: `N noun` and the note about check errors, or the JSON summary. Returns 0. */
int q_finish(long results, char *noun1, char *noun_n)
{
    SB *sb = sb_new();
    if (opt_json) { /* summary extra: ,"results":N */
        sb_cstr(sb, ",\"results\":");
        int_to_dec(sb, results);
        sb_byte(sb, 0);
        Str *extra = str_from_cstr(sb->ptr);
        long files = ld_files()->n;
        summary_emit(1, (int)report_fd, cmd_name(), files, extra);
        return 0;
    }
    if (noun1) { /* type-at prints no count */
        int_to_dec(sb, results);
        sb_cstr(sb, results == 1 ? noun1 : noun_n);
    }
    if (diag_errors) {
        sb_cstr(sb, "note: the program has ");
        int_to_dec(sb, diag_errors);
        sb_cstr(sb, diag_errors == 1 ? " error; answers cover the parts that check\n"
                                     : " errors; answers cover the parts that check\n");
    }
    q_out(sb);
    return 0;
}

/* spec 012: `vo query specs ENTRY`: every @spec ID of the program's tests and properties (not the
 * standard library's or packages'), sorted, each with its tests in source order. */
static int q_specs(void)
{
    Vec *ids = vec_new(), *tests = vec_new(); /* pairs, sorted by ID (stable: source order within one) */
    Vec *files = ld_files();
    for (long f = 0; f < files->n; f++) {
        FileRec *fr = file_get((int)(long)vec_get(files, f));
        if (fr->std || pkg_relpath(fr->path)) continue;
        Vec *items = fr->ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *t = vec_get(items, i);
            if (t->kind != N_TEST) continue;
            for (long a = i - 1; a >= 0 && ((Node *)vec_get(items, a))->kind == N_ANNOTATION; a--) {
                Node *an = vec_get(items, a);
                if (!str_eq_cstr(an->a.n->a.s, "spec")) continue;
                for (long k = 0; k < an->b.v->n; k++) {
                    Str *id = ((Node *)vec_get(an->b.v, k))->a.s;
                    int dup = 0;
                    for (long m = 0; m < ids->n; m++) dup |= vec_get(tests, m) == t && str_eq(vec_get(ids, m), id);
                    if (dup) continue;
                    vec_push(ids, id);
                    vec_push(tests, t);
                    for (long j = ids->n - 1; j > 0 && str_cmp(vec_get(ids, j - 1), id) > 0; j--) {
                        ids->data[j] = ids->data[j - 1];
                        tests->data[j] = tests->data[j - 1];
                        ids->data[j - 1].s = id;
                        tests->data[j - 1].n = t;
                    }
                }
            }
        }
    }
    long distinct = 0;
    for (long i = 0; i < ids->n;) {
        Str *id = vec_get(ids, i);
        long j = i;
        while (j < ids->n && str_eq(vec_get(ids, j), id)) j++;
        SB *sb = sb_new();
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"spec\",\"id\":");
            json_str_into(sb, id);
            sb_cstr(sb, ",\"tests\":[");
        }
        for (long k = i; k < j; k++) {
            Node *t = vec_get(tests, k);
            char *kind = t->flags & NF_PROP ? "prop" : "test";
            if (opt_json) {
                if (k > i) sb_byte(sb, ',');
                sb_cstr(sb, "{\"name\":");
                json_str_into(sb, t->a.s);
                sb_cstr(sb, ",\"kind\":\"");
                sb_cstr(sb, kind);
                sb_cstr(sb, "\",\"file\":");
                json_str_into(sb, file_get((int)t->file)->path);
                sb_cstr(sb, ",\"start\":");
                json_pos(sb, (int)t->file, t->start);
                sb_byte(sb, '}');
            } else {
                sb_str(sb, id);
                sb_cstr(sb, "  ");
                human_loc(sb, (int)t->file, t->start);
                sb_byte(sb, ' ');
                sb_cstr(sb, kind);
                sb_byte(sb, ' ');
                render_str_lit(sb, t->a.s);
                sb_byte(sb, '\n');
            }
        }
        if (opt_json) sb_cstr(sb, "]}\n");
        q_out(sb);
        distinct++;
        i = j;
    }
    return q_finish(distinct, " requirement ID\n", " requirement IDs\n");
}

/* `vo query callers|refs|type-at ENTRY TARGET | specs ENTRY` (contracts/cli.md). */
int cmd_query(void)
{
    char **args = (char **)pos_args;
    if (!pos_n)
        usage_error(str_from_cstr("`query` needs a question: `vo query callers|refs|type-at ENTRY TARGET` or `vo query specs ENTRY`."));
    if (cstr_eq(args[0], "specs")) { /* spec 012 */
        if (pos_n != 2) usage_error(str_from_cstr("`query specs` takes the entry file of the program: `vo query specs main.vo`."));
        q_load(args[1]);
        return q_specs();
    }
    long kind = 0;
    if (cstr_eq(args[0], "callers"))
        kind = 1;
    else if (cstr_eq(args[0], "refs"))
        kind = 2;
    else if (cstr_eq(args[0], "type-at"))
        kind = 3;
    else
        usage_error_arg("Unknown query `$1`. Queries: callers, refs, type-at, specs.", args[0]);
    if (pos_n != 3)
        usage_error_arg("`query $1` takes the entry file of the program and one target: `vo query $1 main.vo TARGET`.",
                        args[0]);
    if (kind == 3) {
        q_load(args[1]);
        long file = 0;
        long off = q_parse_pos(args[2], &file);
        if (off < 0) usage_error_arg("`type-at` needs a position FILE:LINE:COL, not `$1`.", args[2]);
        return q_type_at(file, off);
    }
    q_load(args[1]);
    long keys[2];
    q_resolve(args[2], keys);
    long ka = keys[0], kb = keys[1];
    if (kind == 1 && ka && !(kb == -1 && ((Node *)ka)->kind == N_FN)) /* callers: a function or builtin */
        q_fail(msg("`$1` is not a function; `callers` takes a function or builtin name.", str_from_cstr(args[2]), 0,
                   0));
    Vec *m = q_matches(ka, kb, kind == 1);
    if (kind == 2) q_decl_first(m);
    for (long i = 0; i < m->n; i++) q_ref(vec_get(m, i), args[2]);
    return q_finish(m->n, " reference\n", " references\n");
}

/* ---- vo impact (research R5) ----
 * ponytail: impact follows calls and uses as values only; a change to a record's declaration does
 * not reach the functions that only use the record. Add type-level impact if reviews need it. */

/* T0007 diff_mismatch, exit 64. */
void df_fail(Str *message)
{
    q_fail_code(message, "T0007");
}

/* T0007: the diff's line does not match the file as it is now. */
void df_mismatch(long file, long line)
{
    Str *path = file_get((int)file)->path;
    SB *sb = sb_new(); /* the line number as a str */
    int_to_dec(sb, line);
    Str *num = sb_finish(sb);
    df_fail(msg("The diff does not match `$1` as it is now (line $2): make it again from the current files.", path,
                num, 0));
}

/* The diff says line `line` of the file is p[0..n): T0007 when it is not. */
void df_verify(long file, long line, char *p, long n)
{
    if (file < 0) return;
    if (line <= file_get((int)file)->nlines) {
        long end = line_end((int)file, line);
        long start = line_start((int)file, line);
        if (end - start == n) {
            char *t = file_get((int)file)->text + start;
            long i = 0;
            while (i < n && t[i] == p[i]) i++;
            if (i == n) return;
        }
    }
    df_mismatch(file, line);
}

/* Appends file << 32 | line to the vec held in *vec_global (created on first use). */
void df_push_key(Vec **vec_global, long file, long line)
{
    long key = file << 32 | line;
    if (!*vec_global) *vec_global = vec_new();
    vec_push(*vec_global, (void *)key);
}

/* A removed line `fn NAME(` or `export fn NAME(` names a function the diff may have removed (canonical
 * layout). */
void df_removed_scan(long file, char *p, long n)
{
    if (n >= 7 && mem_eq(p, "export ", 7)) { /* spec 010 */
        p += 7;
        n -= 7;
    }
    if (file < 0 || n < 4 || p[0] != 'f' || p[1] != 'n' || p[2] != ' ') return;
    char *name = p + 3, *end = p + n, *q = name; /* the name: up to `(` or `[` */
    while (q < end && *q != '(' && *q != '[') q++;
    if (q >= end || q == name) return;
    Str *s = str_new(name, q - name);
    W *pair = alloc(16);
    pair[0].i = file;
    pair[1].s = s;
    if (!df_removed) df_removed = vec_new();
    vec_push(df_removed, pair);
}

/* Reads decimal digits at *pp (advancing it); 1 when there was at least one digit. */
long df_number(char **pp, char *end, long *out)
{
    char *p = *pp;
    long v = 0, any = 0;
    while (p < end && (unsigned)(*p - '0') <= 9) {
        v = v * 10 + (*p - '0');
        p++;
        any = 1;
    }
    *pp = p;
    *out = v;
    return any;
}

/* `S[,C]` at *pp; C defaults to 1. T0007 when there is no number. */
void df_range(char **pp, char *end, long *start, long *count)
{
    if (df_number(pp, end, start)) {
        *count = 1;
        if (*pp >= end || **pp != ',') return;
        (*pp)++;
        if (df_number(pp, end, count)) return;
    }
    df_fail(str_from_cstr(M_DF_HUNK));
}

/* 1 when p[0..n) starts with the C string prefix. */
long df_starts(char *p, long n, char *prefix)
{
    for (; *prefix; prefix++, p++, n--)
        if (!n || *p != *prefix) return 0;
    return 1;
}

/* The program file of a `+++ PATH` line's path (p after "+++ "), or -1 (recorded as ignored). */
long df_file(char *p, long n)
{
    char *q = p; /* the path ends at a tab (diff -u's timestamp) */
    while (q < p + n && *q != '\t') q++;
    n = q - p;
    if (df_starts(p, n, "/dev/null")) return -1; /* a deleted file */
    if (n >= 2 && p[1] == '/' && (p[0] == 'a' || p[0] == 'b')) { /* drop one a/ or b/ */
        p += 2;
        n -= 2;
    }
    Str *path = str_new(p, n);
    long file = q_program_file(path);
    if (file >= 0) return file;
    if (!df_ignored) df_ignored = vec_new(); /* not part of the program: reported as ignored */
    vec_push(df_ignored, path);
    return -1;
}

/* Reads a unified diff: the changed new-side lines of each program file (df_plus), the places
 * where lines were removed (df_minus) and the functions a removed line declared (df_removed).
 * Context and added lines must match the current files (T0007). */
void df_parse(char *text, long len)
{
    char *p = text, *end = text + len;
    long file = -1;     /* current program file, or -1 */
    long line = 0;      /* new-side line number */
    long old_left = 0;  /* old lines left in the hunk */
    long new_left = 0;  /* new lines left */
    int saw_file = 0;   /* saw a +++ line */
    while (p < end) {
        char *le = p;
        while (le < end && *le != '\n') le++;
        long n = le - p; /* drop a trailing \r */
        if (n && le[-1] == '\r') n--;
        if (old_left | new_left) { /* in a hunk; an empty line is an empty context line */
            int c = n ? (unsigned char)*p : ' ';
            if (c == ' ') {
                df_verify(file, line, p + 1, n - 1 < 0 ? 0 : n - 1);
                line++;
                old_left--;
                new_left--;
            } else if (c == '+') {
                df_verify(file, line, p + 1, n - 1);
                if (file >= 0) df_push_key(&df_plus, file, line);
                line++;
                new_left--;
            } else if (c == '-') {
                if (file >= 0) {
                    df_push_key(&df_minus, file, line);
                    df_removed_scan(file, p + 1, n - 1);
                }
                old_left--;
            } else if (c != '\\') { /* backslash: "\ No newline at end of file" */
                df_fail(str_from_cstr(M_DF_HUNK));
            }
        } else if (df_starts(p, n, "+++ ")) { /* outside a hunk */
            file = df_file(p + 4, n - 4);
            saw_file = 1;
        } else if (df_starts(p, n, "@@ ")) {
            if (!saw_file) df_fail(str_from_cstr(M_DF_HUNK));
            char *q = p + 3, *lend = p + n; /* @@ -a,b +c,d @@ */
            long start, count;
            if (*q != '-') df_fail(str_from_cstr(M_DF_HUNK));
            q++;
            df_range(&q, lend, &start, &count);
            old_left = count;
            if (q[0] != ' ' || q[1] != '+') df_fail(str_from_cstr(M_DF_HUNK));
            q += 2;
            df_range(&q, lend, &start, &count);
            line = start;
            new_left = count;
            if (!new_left) line++; /* no new lines: the change sits after line c */
        }
        p = le + 1;
    }
    if (!saw_file) df_fail(str_from_cstr("This is not a unified diff: write one with `git diff` or `diff -u`."));
    if (old_left | new_left) df_fail(str_from_cstr(M_DF_HUNK));
}

/* How many keys of vec are in file and in lines [l1, l2] ((l1, l2] when open_start). */
long df_count_in(Vec *v, long file, long l1, long l2, long open_start)
{
    if (!v) return 0;
    if (open_start) l1++;
    long count = 0;
    for (long i = 0; i < v->n; i++) {
        unsigned long key = v->data[i].u;
        long line = (long)(key & 0xffffffffUL);
        if ((long)(key >> 32) == file && line >= l1 && line <= l2) count++;
    }
    return count;
}

/* Appends an impact record {item or name str, role, via} to im_list. */
void im_add(void *item, char *role, void *via)
{
    Impact *e = alloc(sizeof(Impact));
    e->x = item;
    e->role = role;
    e->via = via;
    if (!im_list) im_list = vec_new();
    vec_push(im_list, e);
}

/* 1 when item is already in im_list. */
long im_has(void *item)
{
    for (long i = 0; im_list && i < im_list->n; i++)
        if (((Impact *)im_list->data[i].p)->x == item) return 1;
    return 0;
}

/* changed and added functions, tests and properties, in file and position order. */
void im_items(void)
{
    Vec *files = ld_files();
    for (long i = 0; i < files->n; i++) {
        long file = (long)vec_get(files, i);
        Vec *items = file_get((int)file)->ast->a.v;
        for (long j = 0; j < items->n; j++) {
            Node *it = vec_get(items, j);
            if (it->kind != N_FN && it->kind != N_TEST && it->kind != N_CONST) continue; /* spec 031: constants */
            long l1 = pos_line((int)file, it->start); /* its lines */
            long l2 = pos_line((int)file, (long)it->end - 1);
            long added = df_count_in(df_plus, file, l1, l2, 0); /* added lines inside */
            if (it->kind == N_CONST && df_count_in(df_minus, file, l1, l2, 0)) { /* spec 031: a value edited in place */
                im_add(it, r_changed, 0);
                continue;
            }
            if (added == l2 - l1 + 1) { /* every line is new: added */
                im_add(it, r_added, 0);
                continue;
            }
            if (df_count_in(df_minus, file, l1, l2, 1) + added) im_add(it, r_changed, 0);
        }
    }
}

/* Removed functions: names from removed `fn NAME(` lines that are no longer items of their file. */
void im_removed(void)
{
    for (long i = 0; df_removed && i < df_removed->n; i++) {
        W *pair = vec_get(df_removed, i); /* {file, name} */
        Vec *items = file_get((int)pair[0].i)->ast->a.v;
        int still = 0;
        for (long j = 0; j < items->n && !still; j++) {
            Node *it = vec_get(items, j);
            still = it->kind == N_FN && str_eq(it->a.n->a.s, pair[1].s);
        }
        if (!still) im_add(pair[1].p, r_removed, 0);
    }
}

/* Spec 031: the constant whose value holds node n (a reference made outside any function), or 0. */
static Node *im_const_at(Node *n)
{
    Vec *items = file_get((int)n->file)->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_CONST && it->start <= n->start && n->end <= it->end) return it;
    }
    return 0;
}

/* Breadth first from the changed, added and removed functions: every function, test and
 * property that calls one of them, directly or not (calls and uses as values); spec 031: and every
 * function, test, property and constant that uses a changed constant. */
void im_callers(void)
{
    for (long q = 0; im_list && q < im_list->n; q++) { /* queue position in im_list */
        Impact *e = vec_get(im_list, q);
        void *x = e->x; /* an item, or a removed name (a str) */
        if (e->role == r_test) continue; /* nothing calls a test */
        int is_const = e->role != r_removed && ((Node *)x)->kind == N_CONST;
        for (long k = 0; q_log && k < q_log->n; k++) {
            RefLog *r = vec_get(q_log, k);
            if (r->role != RR_CALL && r->role != RR_VALUE && !(is_const && r->role == RR_USE)) continue;
            Node *by = r->item;
            if (!by && is_const) by = im_const_at(r->node); /* a constant defined from x */
            if (!by) continue;
            if (r->key_a.i) { /* a call of x? (by declaration, or by name when removed) */
                if (r->key_a.p != x || r->key_b.i != -1) continue;
            } else {
                if (r->key_b.u < 4096) continue;
                Node *xn = x; /* x is a str only for removed names */
                if (xn->kind == N_FN || xn->kind == N_TEST) continue;
                if (!str_eq(x, r->key_b.s)) continue;
            }
            if (im_has(by)) continue;
            im_add(by, by->kind == N_TEST ? r_test : r_affected, x);
        }
    }
}

/* The name of an impact entry: an item's name (`test "x"` for tests), or the str itself. */
Str *im_name(void *x)
{
    Node *n = x;
    if (n->kind == N_FN || n->kind == N_TEST) return fx_item_name(n);
    if (n->kind == N_CONST) return n->a.n->a.s; /* spec 031 */
    return x;
}

/* One impact line or record (contracts/reports.md). */
void im_print(void *entry)
{
    Impact *e = entry;
    Node *n = e->x; /* item or str */
    SB *sb = sb_new();
    int is_item = e->role != r_removed && e->role != r_ignored; /* those are names, not items */
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"impact\",\"name\":");
        json_str_into(sb, im_name(n));
        sb_cstr(sb, J_ROLE);
        sb_cstr(sb, e->role);
        sb_byte(sb, '"');
        if (is_item) {
            sb_cstr(sb, ",\"file\":");
            json_str_into(sb, file_get((int)n->file)->path);
            sb_cstr(sb, ",\"start\":");
            json_pos(sb, (int)n->file, n->start);
        }
        if (e->via) {
            sb_cstr(sb, ",\"via\":");
            json_str_into(sb, im_name(e->via));
        }
        sb_cstr(sb, J_CLOSE);
    } else { /* ROLE NAME [via X] [(FILE:LINE)]; a test's name says what it is */
        if (e->role != r_test) {
            sb_cstr(sb, e->role);
            sb_byte(sb, ' ');
        }
        sb_str(sb, im_name(n));
        if (e->via) {
            sb_cstr(sb, " via ");
            sb_str(sb, im_name(e->via));
        }
        if (is_item) {
            sb_cstr(sb, " (");
            sb_str(sb, file_get((int)n->file)->path);
            sb_byte(sb, ':');
            int_to_dec(sb, pos_line((int)n->file, n->start));
            sb_byte(sb, ')');
        }
        sb_byte(sb, '\n');
    }
    q_out(sb);
}

long im_count(char *role)
{
    long n = 0;
    for (long i = 0; im_list && i < im_list->n; i++)
        if (((Impact *)im_list->data[i].p)->role == role) n++;
    return n;
}

/* Appends `key` and the number of entries with this role. */
void im_count_into(SB *sb, char *key, char *role)
{
    long n = im_count(role);
    sb_cstr(sb, key);
    int_to_dec(sb, n);
}

/* Prints im_list entries with this role, or (role 0) every entry that is not changed, added or
 * removed. */
void im_print_role(char *role)
{
    for (long i = 0; im_list && i < im_list->n; i++) {
        Impact *e = vec_get(im_list, i);
        if (role ? e->role != role : (e->role == r_changed || e->role == r_added || e->role == r_removed)) continue;
        im_print(e);
    }
}

/* `vo impact ENTRY --diff FILE` (contracts/cli.md, research R5). */
/* Reads the diff named by --diff (- is stdin) and computes the impact entries of the checked program:
 * changed, added and removed items, then their callers (T0002, T0007 on a bad diff). */
static void im_from_diff(void)
{
    char *path = (char *)opt_diff;
    char *text;
    long len;
    if (path[0] == '-' && !path[1]) { /* standard input to its end: a pipe (`git diff | vo impact`) or a file */
        path = "/dev/stdin";
        SB *sb = sb_new();
        static char buf[65536];
        while ((len = sys_read(0, buf, sizeof buf)) > 0 || len == -4 /* EINTR */)
            if (len > 0) sb_bytes(sb, buf, len);
        text = sb->ptr;
        if (len == 0) len = sb->len;
    } else {
        len = read_whole_file(path, &text);
    }
    if (len < 0) {
        Str *ps = str_from_cstr(path);
        Str *m = msg("Cannot read the diff `$1`.", ps, 0, 0);
        toolchain_error((int)opt_json, (int)report_fd, "T0002", m, ps);
        vo_exit(EXIT_NOINPUT);
    }
    df_parse(text, len);
    im_items();
    im_removed();
    im_callers();
}

/* spec 018: vo test --diff. After the program was checked with q_collect set, the tests and
 * properties the diff reaches in any role (into tests), and the functions it changes or adds (into
 * fns), as declaration nodes in impact order. */
void impact_for_test(Vec *tests, Vec *fns)
{
    im_from_diff();
    for (long i = 0; im_list && i < im_list->n; i++) {
        Impact *e = vec_get(im_list, i);
        if (e->role == r_removed || e->role == r_ignored) continue; /* names, not items */
        Node *x = e->x;
        if (x->kind == N_TEST)
            vec_push(tests, x);
        else if (x->kind == N_FN && (e->role == r_changed || e->role == r_added))
            vec_push(fns, x);
    }
}

int cmd_impact(void)
{
    if (pos_n != 1) usage_error(str_from_cstr("`impact` takes the entry file of the program: `vo impact main.vo --diff FILE`."));
    q_load(((char **)pos_args)[0]);
    im_from_diff();
    for (long i = 0; df_ignored && i < df_ignored->n; i++) /* ignored diff files come last */
        im_add(vec_get(df_ignored, i), r_ignored, 0);
    im_print_role(r_changed); /* changed, added, removed, then the rest */
    im_print_role(r_added);
    im_print_role(r_removed);
    im_print_role(0);
    SB *sb = sb_new(); /* the counts */
    if (opt_json) {
        im_count_into(sb, ",\"changed\":", r_changed);
        im_count_into(sb, ",\"added\":", r_added);
        im_count_into(sb, ",\"removed\":", r_removed);
        im_count_into(sb, ",\"affected\":", r_affected);
        im_count_into(sb, ",\"tests\":", r_test);
        sb_byte(sb, 0);
        Str *extra = str_from_cstr(sb->ptr);
        long files = ld_files()->n;
        summary_emit(1, (int)report_fd, cmd_name(), files, extra);
        return 0;
    }
    im_count_into(sb, "", r_changed);
    im_count_into(sb, " changed, ", r_added);
    im_count_into(sb, " added, ", r_removed);
    im_count_into(sb, " removed, ", r_affected);
    im_count_into(sb, " affected, ", r_test);
    sb_cstr(sb, im_count(r_test) == 1 ? " test\n" : " tests\n");
    q_out(sb);
    q_finish(0, 0, 0);
    return 0;
}

/* Remembers a checked expression (vo query type-at), while q_collect is 1. */
void q_note_expr(Node *node)
{
    if (node->start >= node->end) return;
    if (!q_exprs) q_exprs = vec_new();
    vec_push(q_exprs, node);
}

/* The innermost checked expression whose span contains off, or 0. */
Node *q_expr_at(long file, long off)
{
    Node *best = 0;
    unsigned long best_len = (unsigned long)-1;
    for (long i = 0; q_exprs && i < q_exprs->n; i++) {
        Node *n = q_exprs->data[i].n;
        if (n->file != file || n->start > off || off >= n->end) continue;
        unsigned long len = (unsigned long)n->end - n->start;
        if (len >= best_len) continue;
        best_len = len;
        best = n;
    }
    return best;
}

/* The declaration entry of a target, or 0. */
RefLog *q_decl_of(long key_a, long key_b)
{
    for (long i = 0; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        if (r->role == RR_DECL && r->key_a.i == key_a && r->key_b.i == key_b) return r;
    }
    return 0;
}

/* `vo query type-at`: the innermost name or expression at the position and its type (research R4). */
/* Spec 058 (research R8): `vo query type-at` on a module name (in its import, or before the `.` of a
 * qualified name) answers with the module: its name, file and first comment line. 1 when it answered. */
static int q_module_at(long file, long off)
{
    FileRec *fr = file_get((int)file);
    long start = -1, end = -1, target = -1;
    Vec *items = fr->ast ? fr->ast->a.v : 0;
    for (long i = 0; items && i < items->n && start < 0; i++) { /* the module name of an import */
        Node *imp = vec_get(items, i);
        if (imp->kind != N_IMPORT || imp->c.i || !imp->d.i || !imp->b.n) continue;
        if (off >= (long)imp->b.n->start && off < (long)imp->b.n->end) {
            start = imp->b.n->start;
            end = imp->b.n->end;
            target = imp->d.i - 1;
        }
    }
    RefLog *r = start < 0 ? q_entry_at(file, off) : 0;
    if (r) { /* the module part of `m.name` */
        Node *n = r->node;
        long dot = -1;
        for (long k = n->start; k < (long)n->end && dot < 0; k++)
            if (fr->text[k] == '.') dot = k;
        if (dot < 0 || off >= dot) return 0;
        Str *m = str_new(fr->text + n->start, dot - n->start);
        for (Sym *s = fr->scope; s && target < 0; s = s->next)
            if (s->kind == SYM_MODULE && str_eq(s->name, m)) target = s->slot;
        start = n->start;
        end = dot;
    }
    if (target < 0) return 0;
    Str *mod = doc_module_name(target), *line = doc_module_line(target);
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"type_at\",\"file\":");
        json_span(sb, (int)file, start, end);
        sb_cstr(sb, ",\"text\":");
        json_src_text(sb, (int)file, start, end);
        sb_cstr(sb, ",\"type\":\"module\",\"name\":");
        json_src_text(sb, (int)file, start, end);
        sb_cstr(sb, ",\"declaration\":{\"file\":");
        json_span(sb, (int)target, 0, 0);
        sb_cstr(sb, "},\"doc\":{\"module\":");
        json_str_into(sb, mod);
        sb_cstr(sb, ",\"item\":\"module\",\"description\":");
        json_str_into(sb, line);
        sb_cstr(sb, "}" J_CLOSE);
    } else {
        human_loc(sb, (int)file, start);
        sb_byte(sb, ' ');
        sb_bytes(sb, fr->text + start, end - start);
        sb_cstr(sb, ": module ");
        sb_str(sb, mod);
        sb_cstr(sb, " (declared at ");
        human_loc(sb, (int)target, 0);
        sb_cstr(sb, ")\n");
        if (line->len) {
            sb_cstr(sb, "    ");
            sb_str(sb, line);
            sb_byte(sb, '\n');
        }
    }
    q_out(sb);
    return 1;
}

/* The type of a name the checker gave none, as its uses have it: a function's name is the function as
 * a value, a record's or variant's name that type, and a case without a value its variant (as
 * `Shape.dot` is); else 0. */
static Type *q_decl_type(RefLog *r)
{
    Node *d = (Node *)r->key_a.i;
    if (!d) return 0; /* an unresolved name */
    if (r->key_b.i == -1 && d->kind == N_FN) return ty_func_of(d);
    if (r->key_b.i == -1 && (d->kind == N_RECORD || d->kind == N_VARIANT)) return decl_type(d);
    if (r->key_b.i >= 0 && d->kind == N_VARIANT) return decl_type(d); /* (variant, case) */
    return 0;
}

int q_type_at(long file, long off)
{
    ty_name_file = file; /* spec 058: types as this file writes them (`list[pricing.Item]`) */
    if (q_module_at(file, off)) return q_finish(1, 0, 0);
    RefLog *name = q_entry_at(file, off); /* a name there, or 0 */
    Node *expr = q_expr_at(file, off);    /* an expression there, or 0 */
    RefLog *decl = 0;                     /* the declaration entry */
    Type *t;
    Node *shown; /* the node reported */
    Node *cdecl = 0; /* spec 031: the constant the name refers to, or 0 */
    if (!name && !expr) q_fail(str_from_cstr("Nothing to look up at this position: point at a name or an expression."));
    if (name && (!expr || (long)name->node->end - name->node->start <= (long)expr->end - expr->start)) {
        decl = q_decl_of(name->key_a.i, name->key_b.i); /* the smaller span wins; a name on a tie */
        t = name->type; /* the type: declared here, the checked expression, or the declaration's */
        Node *item = name->key_b.i < 0 ? (Node *)name->key_a.i : 0;
        if (item && item->kind == N_CONST) { /* spec 031: a constant's type and value */
            cdecl = item;
            if (!t) t = item->type;
        }
        if (!t && expr && name->node == expr) t = expr->type;
        if (!t && decl) t = decl->type;
        if (!t && expr) t = expr->type;
        if (!t) t = q_decl_type(name);
        shown = name->node;
    } else {
        t = expr->type;
        name = 0;
        shown = expr;
    }
    int f = (int)shown->file;
    SB *doc = 0; /* spec 023: the doc record of the item the name refers to */
    Str *doc_sig = 0, *doc_desc = 0;
    if (name) {
        doc = sb_new();
        if (!doc_record_for(doc, name->key_a.i, name->key_b.i, &doc_sig, &doc_desc)) doc = 0;
    }
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"type_at\",\"file\":");
        json_span(sb, f, shown->start, shown->end);
        sb_cstr(sb, ",\"text\":");
        json_src_text(sb, f, shown->start, shown->end);
        sb_cstr(sb, ",\"type\":");
        json_str_into(sb, q_type_name(t));
        if (cdecl && !ty_bad(cdecl->type)) { /* spec 031: the constant's value, in vo syntax */
            SB *v = sb_new();
            render_value(v, cdecl->c.n->a.i, cdecl->type);
            sb_cstr(sb, ",\"value\":");
            json_str_into(sb, sb_finish(v));
        }
        if (name) {
            sb_cstr(sb, ",\"name\":");
            json_src_text(sb, f, shown->start, shown->end);
            if (decl) {
                sb_cstr(sb, ",\"declaration\":{\"file\":");
                Node *dn = decl->node;
                json_span(sb, (int)dn->file, dn->start, dn->end);
                sb_byte(sb, '}');
            }
            if (doc) {
                sb_cstr(sb, ",\"doc\":{");
                sb_bytes(sb, doc->ptr, doc->len);
                sb_byte(sb, '}');
            }
        }
        sb_cstr(sb, J_CLOSE);
    } else { /* FILE:LINE:COL TEXT: TYPE (declared at FILE:LINE:COL) */
        human_loc(sb, f, shown->start);
        sb_byte(sb, ' ');
        sb_bytes(sb, file_get(f)->text + shown->start, (long)shown->end - shown->start);
        sb_cstr(sb, ": ");
        sb_str(sb, q_type_name(t));
        if (cdecl && !ty_bad(cdecl->type)) { /* spec 031: = value */
            sb_cstr(sb, " = ");
            render_value(sb, cdecl->c.n->a.i, cdecl->type);
        }
        if (decl) {
            sb_cstr(sb, " (declared at ");
            human_loc(sb, (int)decl->node->file, decl->node->start);
            sb_byte(sb, ')');
        }
        sb_byte(sb, '\n');
        if (doc && doc_sig) { /* spec 023: the signature and the description, indented */
            sb_cstr(sb, "    ");
            sb_str(sb, doc_sig);
            sb_byte(sb, '\n');
            if (doc_desc) {
                sb_cstr(sb, "    ");
                sb_str(sb, doc_desc);
                sb_byte(sb, '\n');
            }
        }
    }
    q_out(sb);
    return q_finish(1, 0, 0);
}

Str *q_type_name(Type *t)
{
    return t ? ty_name(t) : str_from_cstr("?");
}
