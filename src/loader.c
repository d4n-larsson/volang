/* Programs across files: import resolution, loading, cycle detection, binding (contracts/grammar.md
 * §Imports; ported from src/loader.s, spec 008). */
#include "vo.h"

#define FS_NEW 0
#define FS_LOADING 1
#define FS_LOADED 2
#define FS_CHECKED 3
static Vec *mm_names, *mm_diags; /* spec 058: modules reported missing (E0204), and their reports */
#define NO_PACKAGE ((void *)-1) /* pkg_import_path: a package that failed its check */

static Vec *ld_order; /* files in dependency order (dependencies first) */
static Vec *ex_decls; /* spec 010: declarations already reported as needing `export` ... */
static Vec *ex_diags; /* ... and the diagnostic that carries the fix for each */
static Vec *ld_stack; /* import nodes on the current DFS path */
static Str *ld_tree;  /* spec 013 (M4): the program's tree (the entry file's directory, resolved) */

/* 1 when the file at path, links followed, lies in the program's tree or a listed package (or does
 * not exist: reported as missing instead). */
static int inside_tree(Str *path)
{
    if (ld_app) return 1; /* spec 034: only embedded files can load */
    long fd = sys_open(path->data, O_PATH | O_CLOEXEC);
    if (fd < 0) return 1;
    sys_close((int)fd);
    Str *real = rt_resolve(path);
    return real && ((ld_tree && path_under(real, ld_tree)) || pkg_contains(real));
}
long ld_hard;         /* a loaded file had a hard syntax error */

/* The id of the registered file with this path, or -1. */
long file_find(Str *path)
{
    for (long i = 0; i < file_count; i++) {
        FileRec *f = file_get((int)i);
        if (f->gen == cur_gen && str_eq(f->path, path)) return i; /* spec 006: the current generation */
    }
    return -1;
}

/* The imported file relative to the importer's directory: "a/b.vo" for `import a.b;` (spec 058), and for
 * the 0.3 form `import a.b.name;` (marked by load_imports: N_C 1) the file without its last segment. */
Str *import_rel(Node *import)
{
    SB *sb = sb_new();
    for (long i = 0; i < import->a.v->n - (import->c.i ? 1 : 0); i++) {
        if (i) sb_byte(sb, '/');
        sb_str(sb, vec_get(import->a.v, i));
    }
    sb_cstr(sb, ".vo");
    return sb_finish(sb);
}

/* The path an import loads (spec 007 research R2, R8): `std/MODULE.vo` for `import std.MODULE.x;`,
 * a package's directory for a listed package name, else rel under the importer's directory. */
Str *import_path(Node *import, Str *rel, FileRec *importer)
{
    if (str_eq_cstr(vec_get(import->a.v, 0), "std")) return rel; /* the embedded file's own path */
    Str *p = pkg_import_path(import, importer);                 /* 0 when the first segment is none */
    if (p) return p;
    return str_join2(importer->dir, rel);
}

/* For a path under std/: registers the embedded file (FILE_STD) and returns its id, or -1 when
 * there is no such module; 0 for any other path (read from the disk). */
long std_lookup(Str *path)
{
    if (path->len < 4 || path->data[0] != 's' || path->data[1] != 't' || path->data[2] != 'd' ||
        path->data[3] != '/')
        return 0;
    for (const StdFile *sf = std_files; sf->path; sf++) {
        if (!str_eq_cstr(path, (char *)sf->path)) continue;
        int id = file_add(path, (char *)sf->text, sf->len);
        file_get(id)->std = 1;
        return id;
    }
    return -1;
}

/* Before a program runs: clears the declared effect sets of standard-library functions, so their
 * frames take their caller's grant (as anonymous functions do) and scopes such as net("localhost")
 * keep applying inside the library (spec 007 research R6). The checker is done with them. */
void std_runtime_grants(void)
{
    for (long i = 0; i < file_count; i++) {
        FileRec *f = file_get((int)i);
        if (!f->std || !f->ast) continue;
        Vec *items = f->ast->a.v;
        for (long j = 0; j < items->n; j++) {
            Node *it = vec_get(items, j);
            if (it->kind != N_FN) continue;
            Fx *fx = item_fx(it);
            if (fx) fx->declared = 0;
        }
    }
}

Str *str_join2(Str *a, Str *b)
{
    SB *sb = sb_new();
    sb_str(sb, a);
    sb_str(sb, b);
    return sb_finish(sb);
}

/* Registers, lexes and parses a file once; returns its id (-1 when it cannot be read). */
long load_file(Str *path)
{
    long id = file_find(path);
    if (id >= 0) {
        if (file_get((int)id)->ast) return id;
        /* only lexed by a missing-import scan: lex again (reported) and parse */
    } else {
        id = std_lookup(path); /* spec 007: std/MODULE.vo comes from the binary, never the disk */
        if (id < 0) return -1;
        if (!id) {
            if (ld_app) return -1; /* spec 034: a built executable reads no source from the disk */
            char *text;
            long len = pkg_cached(path, &text); /* spec 013 (H5, H6): a package file: the bytes that were hashed */
            if (len == -2) return -1;
            if (len < 0) len = read_whole_file(path->data, &text);
            if (len < 0) return -1;
            id = file_add(path, text, len);
        }
    }
    lex((int)id);
    parse_file((int)id);
    if (p_hard) ld_hard = p_hard;
    return id;
}

/* Spec 058: whether an import's file exists (embedded std file, cached package file, or on the disk),
 * without loading it. */
static int import_exists(Str *path)
{
    if (file_find(path) >= 0) return 1;
    if (path->len >= 4 && !memcmp(path->data, "std/", 4)) {
        for (const StdFile *sf = std_files; sf->path; sf++)
            if (str_eq_cstr(path, (char *)sf->path)) return 1;
        return 0;
    }
    if (ld_app) return 0;
    char *text;
    long len = pkg_cached(path, &text);
    if (len >= 0) return 1;
    if (len == -2) return 0;
    long fd = sys_open(path->data, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 0;
    sys_close((int)fd);
    return 1;
}

/* E0206 at the import that closes a cycle; the message and notes follow the chain. */
void report_cycle(Node *import, long target)
{
    Vec *path = ld_stack; /* find where the target file entered the path */
    long from = 0;
    while (from < path->n && ((Node *)vec_get(path, from))->file != target) from++;
    SB *sb = sb_new(); /* "a.vo -> b.vo -> a.vo" */
    sb_str(sb, file_get((int)target)->path);
    for (long i = from; i < path->n; i++) {
        Node *imp = vec_get(path, i);
        FileRec *f = file_get((int)(imp->d.i - 1)); /* target file + 1 */
        sb_cstr(sb, " -> ");
        sb_str(sb, f->path);
    }
    sb_cstr(sb, " -> ");
    sb_str(sb, file_get((int)target)->path);
    Diag *d = ck_err("E0206", import, msg("Import cycle: $1.", sb_finish(sb), 0, 0));
    for (long i = from; i < path->n; i++) { /* one note per import on the cycle */
        Node *imp = vec_get(path, i);
        Str *m = msg("`$1` is imported here.", file_get((int)(imp->d.i - 1))->path, 0, 0);
        diag_add_related(d, (int)imp->file, imp->start, imp->end, m);
    }
}

/* Depth-first: loads every imported file, reports missing files and cycles, and appends files
 * to ld_order after their dependencies. */
void load_imports(int file)
{
    FileRec *f = file_get(file);
    f->state = FS_LOADING;
    Vec *items = f->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *imp = vec_get(items, i);
        if (imp->kind != N_IMPORT) continue;
        imp->d.i = 0;
        imp->c.i = 0;
        long nseg = imp->a.v->n;
        if (!nseg || !imp->b.n) continue; /* `import ;`: the parser reported it */
        Str *last = vec_get(imp->a.v, nseg - 1);
        int is_std = str_eq_cstr(vec_get(imp->a.v, 0), "std");
        if (nseg == 1 && is_std) {
            ck_err("E0205", imp, str_from_cstr("An import names a file: `import std.json;`."));
            continue;
        }
        /* spec 058 (research R3): `import a.b.c;` is the module a/b/c.vo; the 0.3 form names an item of
         * a/b.vo: a capitalized last segment, or no a/b/c.vo while a/b.vo exists */
        int old_ok = nseg >= (is_std || pkg_is_name(vec_get(imp->a.v, 0), f) ? 3 : 2);
        if (old_ok && last->len && last->data[0] >= 'A' && last->data[0] <= 'Z') imp->c.i = 1;
        if (is_std && nseg >= 3) imp->c.i = 1; /* std modules are flat: std.a.b names the item b of std/a.vo */
        Str *rel = import_rel(imp);
        Str *path = import_path(imp, rel, f); /* spec 007: std/ and package roots */
        if ((void *)path == NO_PACKAGE) continue; /* E0701/E0702, reported */
        if (!imp->c.i && old_ok && path && !import_exists(path)) {
            imp->c.i = 1; /* a/b/c.vo -> a/b.vo (derived, so a package is resolved and verified once) */
            Str *rel1 = import_rel(imp);
            long cut = last->len + 4; /* "/c.vo" */
            Str *path1 = path->len > cut ? str_join2_c(str_new(path->data, path->len - cut), ".vo") : 0;
            if (path1 && import_exists(path1)) {
                rel = rel1;
                path = path1;
            } else {
                imp->c.i = 0; /* neither: report the module's file */
            }
        }
        if (!str_eq_cstr(vec_get(imp->a.v, 0), "std") && !inside_tree(path)) { /* (std is embedded) */
            ck_err("E0211", imp,
                   msg("`$1` leads outside this program's directory through a symbolic link: an import must "
                       "stay inside it (or inside a listed package).",
                       path, 0, 0));
            continue;
        }
        long target = load_file(path);
        if (target < 0) { /* missing file: E0205 */
            ck_err("E0205", imp, msg("Cannot find `$1` for this import.", rel, 0, 0));
            continue;
        }
        imp->d.i = target + 1;
        long state = file_get((int)target)->state;
        if (state == FS_LOADING) { /* back to a file on the current path: a cycle */
            report_cycle(imp, target);
            imp->d.i = 0;
            continue;
        }
        if (state != FS_NEW) continue;
        vec_push(ld_stack, imp);
        load_imports((int)target);
        ld_stack->n--;
    }
    f->state = FS_LOADED;
    vec_push(ld_order, (void *)(long)file);
}

/* spec 018: 1 for a standard library or package file (coverage and mutation leave them out). */
int file_is_library(int file)
{
    FileRec *f = file_get(file);
    return f->std || pkg_relpath(f->path) != 0;
}

/* The package name when the file lies in a listed package ("@NAME/a/b.vo"), else 0. */
static Str *package_of(FileRec *f)
{
    Str *rel = f->std ? 0 : pkg_relpath(f->path);
    if (!rel) return 0;
    long n = 1;
    while (n < rel->len && rel->data[n] != '/') n++;
    return str_new(rel->data + 1, n - 1);
}

/* E0209/E0210 at `at` for a declaration that needs `export` (spec 010 research R2-R4): the first report
 * for decl carries the fix (none for a library or package file, which a program cannot change); later
 * reports become related notes on it, so applying every first fix never inserts `export` twice. */
void export_needed(char *code, Node *at, Node *decl, Str *message)
{
    for (long i = 0; i < ex_decls->n; i++)
        if (vec_get(ex_decls, i) == decl) {
            diag_add_related(vec_get(ex_diags, i), (int)at->file, at->start, at->end, str_from_cstr("Also needed here."));
            return;
        }
    Diag *d = ck_err(code, at, message);
    FileRec *f = file_get((int)decl->file);
    if (!f->std && !package_of(f)) diag_add_fix(d, (int)decl->file, decl->start, decl->start, str_from_cstr("export "));
    vec_push(ex_decls, decl);
    vec_push(ex_diags, d);
}

/* E0209 for an import of an item declared without `export`; the import still binds (no cascade). */
static void check_exported(Node *nn, Node *decl, long target)
{
    if (decl->flags & NF_EXPORT) return;
    FileRec *f = file_get((int)target);
    Str *m;
    if (f->std) { /* "std/json.vo" -> std.json */
        Str *mod = str_new(f->path->data + 4, f->path->len - 7);
        m = msg("`$1` is internal to `std.$2`.", nn->a.s, mod, 0);
    } else if (package_of(f)) {
        m = msg("`$1` is internal to package `$2`.", nn->a.s, package_of(f), 0);
    } else {
        m = msg("`$1` is not exported by `$2`.", nn->a.s, f->path, 0);
    }
    export_needed("E0209", nn, decl, m);
}

/* ---- spec 058: E0215, the 0.3 form `import a.b.item;`, and its fix (research R6) ---- */

/* 1 when a program file (not std, not a package) has 0.3 imports: checking it keeps the reference log. */
int file_has_old_imports(int file)
{
    FileRec *f = file_get(file);
    if (f->std || !f->ast) return 0;
    Str *real = rt_resolve(f->path); /* a package's file is one, however the path that reached it reads */
    if (pkg_relpath(f->path) || (real && pkg_relpath(real))) return 0;
    Vec *items = f->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *imp = vec_get(items, i);
        if (imp->kind == N_IMPORT && imp->c.i && imp->d.i && imp->b.n) return 1;
    }
    return 0;
}

/* The text of an import's module: `std.json` for `import std.json.parse_json;` (0.3) or `import std.json;`. */
static Str *module_text(Node *imp)
{
    SB *sb = sb_new();
    long n = imp->a.v->n - (imp->c.i ? 1 : 0);
    for (long i = 0; i < n; i++) {
        if (i) sb_byte(sb, '.');
        sb_str(sb, vec_get(imp->a.v, i));
    }
    return sb_finish(sb);
}

typedef struct MigEdit {
    long start, end;
    Str *text;
} MigEdit;

static void mig_edit(Vec *edits, long start, long end, Str *text)
{
    for (long i = 0; i < edits->n; i++) {
        MigEdit *e = vec_get(edits, i);
        if (e->start == start && e->end == end) return; /* the same node logged twice */
    }
    MigEdit *e = alloc(sizeof(MigEdit));
    e->start = start;
    e->end = end;
    e->text = text;
    vec_push(edits, e);
}

/* After a file with 0.3 imports was checked with the reference log on (entries from log_start): E0215 at
 * its first such import, with one fix from the first import to the last changed use: the imports become
 * one per module (sorted as `vo fmt` sorts them), each use of an item they bound is written `m.name`, and
 * each local named like a module the fix introduces is renamed `NAME_value`. When an import names a std
 * item changed in vo 0.2 (E0214), the E0214 fixes go first: E0215 then has no fix in this round. */
void migrate_imports(int file, long log_start)
{
    FileRec *f = file_get(file);
    Vec *items = f->ast->a.v;
    Vec *imports = vec_new();
    Node *first_old = 0;
    int changed = 0;
    Vec *decls = vec_new(), *quals = vec_new(); /* item declarations bound by 0.3 imports -> `m.name` */
    Vec *mods = vec_new();                      /* module names the fix introduces */
    for (long i = 0; i < items->n; i++) {
        Node *imp = vec_get(items, i);
        if (imp->kind != N_IMPORT) continue;
        vec_push(imports, imp);
        if (!imp->c.i || !imp->d.i || !imp->b.n) continue;
        if (!first_old) first_old = imp;
        long target = imp->d.i - 1;
        FileRec *tf = file_get((int)target);
        Str *name = imp->b.n->a.s;
        if (tf->std && libchange_binds(libchange_find(str_new(tf->path->data + 4, tf->path->len - 7), name))) changed = 1;
        Str *m = vec_get(imp->a.v, imp->a.v->n - 2);
        vec_push(mods, m);
        for (Sym *s = tf->scope; s; s = s->next)
            if (s->decl && s->decl->file == target && s->kind != SYM_LOCAL && s->kind != SYM_MODULE && str_eq(s->name, name)) {
                vec_push(decls, s->decl);
                vec_push(quals, msg("$1.$2", m, name, 0));
                break;
            }
    }
    if (!first_old) return;
    Node *nn = first_old->b.n;
    Diag *d = ck_err("E0215", first_old, msg("Imports name a module, not an item: `import $1;`, then `$2.$3`.",
                                             module_text(first_old), vec_get(first_old->a.v, first_old->a.v->n - 2),
                                             nn->a.s));
    if (changed) return;
    Vec *edits = vec_new();
    /* locals and parameters named like an introduced module: their declaring name nodes and new names */
    Vec *lkeys = vec_new(), *lnames = vec_new();
    for (long i = log_start; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        if (r->role != RR_DECL || (long)r->node->file != file || r->key_a.i != (long)r->node) continue;
        for (long k = 0; k < mods->n; k++) {
            if (!str_eq(r->node->a.s, vec_get(mods, k))) continue;
            Str *fresh = msg("$1_value", r->node->a.s, 0, 0);
            for (long j = 2;; j++) { /* not a name the file already declares */
                int taken = 0;
                for (long q = log_start; q < q_log->n && !taken; q++) {
                    RefLog *o = vec_get(q_log, q);
                    if (o->role == RR_DECL && (long)o->node->file == file && o->node->a.s && str_eq(o->node->a.s, fresh)) taken = 1;
                }
                if (!taken) break;
                fresh = msg("$1_value$2", r->node->a.s, int_to_str(j), 0);
            }
            vec_push(lkeys, r->node);
            vec_push(lnames, fresh);
            break;
        }
    }
    for (long i = log_start; q_log && i < q_log->n; i++) {
        RefLog *r = vec_get(q_log, i);
        Node *n = r->node;
        if ((long)n->file != file || r->role == RR_IMPORT) continue;
        for (long k = 0; k < lkeys->n; k++)
            if (r->key_a.i == (long)vec_get(lkeys, k)) mig_edit(edits, n->start, n->end, vec_get(lnames, k));
        if (r->role == RR_DECL || r->key_b.i != -1) continue;
        for (long k = 0; k < decls->n; k++) {
            if (r->key_a.i != (long)vec_get(decls, k)) continue;
            Str *q = vec_get(quals, k);
            long dot = str_dot(q); /* only where the bare name is written */
            if (n->end - n->start == q->len - dot - 1 && !memcmp(f->text + n->start, q->data + dot + 1, (size_t)(q->len - dot - 1)))
                mig_edit(edits, n->start, n->end, q);
        }
    }
    /* the imports: one block in place of the first, the other lines removed */
    Vec *texts = vec_new();
    for (long i = 0; i < imports->n; i++) {
        Node *imp = vec_get(imports, i);
        Str *t = module_text(imp);
        int dup = 0;
        for (long k = 0; k < texts->n; k++) dup |= str_eq(vec_get(texts, k), t);
        if (dup) continue;
        long j = texts->n; /* insertion sort, as vo fmt orders imports */
        vec_push(texts, t);
        while (j > 0 && str_cmp(vec_get(texts, j - 1), t) > 0) {
            texts->data[j] = texts->data[j - 1];
            j--;
        }
        texts->data[j].s = t;
    }
    SB *block = sb_new();
    for (long k = 0; k < texts->n; k++) {
        if (k) sb_byte(block, '\n');
        sb_cstr(block, "import ");
        sb_str(block, vec_get(texts, k));
        sb_byte(block, ';');
    }
    Node *head = vec_get(imports, 0);
    mig_edit(edits, head->start, head->end, sb_finish(block));
    for (long i = 1; i < imports->n; i++) {
        Node *imp = vec_get(imports, i);
        long s = imp->start, e = imp->end;
        while (s > 0 && f->text[s - 1] != '\n') s--; /* the whole line */
        if (e < f->len && f->text[e] == '\n') e++;
        mig_edit(edits, s, e, str_from_cstr(""));
    }
    for (long i = 1; i < edits->n; i++) { /* by position */
        MigEdit *e = vec_get(edits, i);
        long j = i - 1;
        while (j >= 0 && ((MigEdit *)vec_get(edits, j))->start > e->start) {
            edits->data[j + 1] = edits->data[j];
            j--;
        }
        edits->data[j + 1].p = e;
    }
    long from = head->start, to = from;
    SB *sb = sb_new();
    for (long i = 0; i < edits->n; i++) {
        MigEdit *e = vec_get(edits, i);
        if (e->start < to) continue; /* overlapping: the earlier one wins */
        sb_bytes(sb, f->text + to, e->start - to);
        sb_str(sb, e->text);
        to = e->end;
    }
    diag_add_fix(d, file, from, to, sb_finish(sb));
}

/* Spec 058: W0203 for a module import the file never uses (the parser counted the uses in N_E), with the
 * fix removing the line. */
void imports_report_unused(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *imp = vec_get(items, i);
        if (imp->kind != N_IMPORT || imp->c.i || !imp->d.i || imp->e.i || !imp->b.n) continue;
        Diag *d = ck_warn("W0203", imp, msg("`$1` is imported but not used.", module_label(imp->d.i - 1), 0, 0));
        FileRec *f = file_get((int)imp->file);
        long end = imp->end;
        if (end < f->len && f->text[end] == '\n') end++;
        diag_add_fix(d, (int)imp->file, imp->start, end, str_from_cstr(""));
    }
}

/* Spec 058: the declaration of a private top-level item `name` of the file, or 0. */
Node *module_private_item(long target, Str *name)
{
    for (Sym *s = file_get((int)target)->scope; s; s = s->next)
        if (s->decl && s->decl->file == target && s->kind != SYM_LOCAL && s->kind != SYM_MODULE &&
            !(s->decl->flags & NF_EXPORT) && str_eq(s->name, name))
            return s->decl;
    return 0;
}

/* Spec 058: E0209 for `m.name` naming a private item (the fix adds `export`, as for an import). */
void private_item_used(Node *at, Str *name, Node *decl, long target)
{
    Node nn = *at;
    nn.a.s = name;
    check_exported(&nn, decl, target);
}

/* Spec 058 (research R2): binds the module name (SYM_MODULE) and every exported top-level item of the
 * target as `m.name`; E0202 when the name is already a module or an item of this file. */
static void bind_module(Node *imp, Node *nn, long target)
{
    imp->e.i++; /* any report below stands for W0203 too */
    Sym *prev = scope_find(nn->a.s);
    if (prev && !is_placeholder(prev)) {
        if (prev->kind == SYM_MODULE && prev->slot == target) { /* the same import again: remove it */
            Diag *d = ck_err1("E0202", nn, "`$1` is already imported.", nn->a.s);
            FileRec *f = file_get((int)imp->file);
            long end = imp->end;
            if (end < f->len && f->text[end] == '\n') end++;
            diag_add_fix(d, (int)imp->file, imp->start, end, str_from_cstr(""));
        } else if (prev->kind == SYM_MODULE) {
            ck_err("E0202", nn, msg("`$1` is already a module of this file (`$2`).", nn->a.s,
                                    file_get((int)prev->slot)->path, 0));
        } else {
            ck_err1("E0202", nn, "`$1` is already defined.", nn->a.s);
        }
        return;
    }
    if (file_defines((int)imp->file, nn->a.s)) { /* an item of this file has the module's name: the item wins */
        ck_err("E0202", nn, msg("`$1` is also an item of this file: rename the item or the module's file.", nn->a.s, 0, 0));
        return;
    }
    imp->e.i--;
    Sym *ms = sym_new(nn->a.s, SYM_MODULE, 0, 0, nn);
    ms->slot = target;
    ms->decl = imp; /* (after sym_new: the reference log keeps no declaration for a module) */
    scope_add(ms);
    FileRec *tf = file_get((int)target);
    long qc = q_collect; /* the items' bindings are not references (the import names the module) */
    q_collect = 0;
    for (Sym *s = tf->scope; s; s = s->next) {
        if (!s->decl || s->decl->file != target || s->kind == SYM_LOCAL || s->kind == SYM_MODULE) continue;
        if (!(s->decl->flags & NF_EXPORT)) continue; /* a private item: E0209 at its use (report_undefined) */
        SB *sb = sb_new();
        sb_str(sb, nn->a.s);
        sb_byte(sb, '.');
        sb_str(sb, s->name);
        scope_add(sym_new(sb_finish(sb), s->kind, s->decl, s->type, nn));
    }
    q_collect = qc;
}

/* Adds each imported item (a top-level fn, record or variant of the target file) to the scope. */
void bind_imports(int file)
{
    Vec *items = file_get(file)->ast->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *imp = vec_get(items, i);
        if (imp->kind != N_IMPORT) continue;
        if (!imp->d.i) { /* unresolved (already reported): a silent placeholder, no cascade */
            if (imp->b.n) bind_placeholder(imp->b.n);
            continue;
        }
        long target = imp->d.i - 1;
        Node *nn = imp->b.n; /* name node */
        FileRec *tf = file_get((int)target);
        if (!imp->c.i) { /* spec 058: a module: its name, and each exported item under `m.name` */
            bind_module(imp, nn, target);
            continue;
        }
        const LibChange *change = 0; /* spec 056: a changed std item: E0214, bound to what replaces it */
        Str *want = nn->a.s;
        if (tf->std) {
            change = libchange_find(str_new(tf->path->data + 4, tf->path->len - 7), nn->a.s);
            if (!libchange_binds(change)) change = 0;
            if (change && libchange_new_name(change)) want = libchange_new_name(change);
        }
        Sym *s = tf->scope; /* search the target's own top-level items */
        for (; s; s = s->next)
            if (s->decl && s->decl->file == target && s->kind != SYM_LOCAL && str_eq(s->name, want)) break;
        if (change && s) {
            libchange_import(nn, 0, change);
            if (scope_find(nn->a.s)) continue; /* (both the old and the new name imported: one report) */
            Sym *b = sym_new(nn->a.s, s->kind, s->decl, s->type, nn);
            b->flags |= SYF_LIBCHANGE;
            b->change = change;
            scope_add(b);
            continue;
        }
        if (!s) { /* no such item: E0205 naming the file */
            ck_err("E0205", nn, msg("`$1` has no item `$2`.", file_get((int)target)->path, nn->a.s, 0));
            bind_placeholder(nn);
            continue;
        }
        check_exported(nn, s->decl, target);
        if (scope_find(nn->a.s)) { /* duplicate with something already visible? */
            ck_err1("E0202", nn, "`$1` is already defined.", nn->a.s);
            continue;
        }
        scope_add(sym_new(s->name, s->kind, s->decl, s->type, nn));
    }
}

/* The files of the last checked program (file ids, dependencies first). */
Vec *ld_files(void)
{
    return ld_order;
}

/* Loads the entry file and everything it imports, then checks them, dependencies first.
 * mode 0 = check, 1 = run (main required in the entry), 2 = test. */
void check_program(int file, int mode)
{
    ck_init();
    fx_program_begin();
    FileRec *f = file_get(file);
    if (f->state == FS_CHECKED) return; /* already checked as another file's import */
    if (!f->ast) {
        lex(file);
        parse_file(file);
        ld_hard = p_hard;
    }
    ld_order = vec_new();
    ld_stack = vec_new();
    ex_decls = vec_new();
    mm_names = 0;
    ex_diags = vec_new();
    ld_tree = rt_resolve(f->dir);
    load_imports(file);
    if (ld_hard) return;
    web_inputs = vec_new(); /* spec 029: std.web operations' input records, for @header */
    for (long i = 0; i < ld_order->n; i++) {
        int id = (int)(long)vec_get(ld_order, i);
        FileRec *g = file_get(id);
        if (g->state == FS_CHECKED) continue;
        g->state = FS_CHECKED;
        check_items(id, id == file ? mode : 0);
    }
    check_web_headers(ld_order);
    ty_name_file = -1; /* spec 058: later reports (runtime frames, docs) print declared names */
}

/* A silent symbol for a name whose import failed (later uses are not reported). */
void bind_placeholder(Node *name_node)
{
    Sym *s = sym_new(name_node->a.s, SYM_LOCAL, 0, t_error, 0);
    s->flags = SYF_USED;
    s->slot = -1;
    scope_add(s);
}

/* 1 when the file (lexed) declares a top-level fn, record, variant or constant with this name. */
int file_defines(int file, Str *name)
{
    FileRec *f = file_get(file);
    long depth = 0; /* brace depth */
    for (Token *t = f->tokens; t->kind; t++) {
        if (t->kind == T_LBRACE) {
            depth++;
        } else if (t->kind == T_RBRACE) {
            depth--;
        } else if (!depth && (t->kind == T_FN || t->kind == T_RECORD || t->kind == T_VARIANT ||
                              (t->kind == T_IDENT && str_eq_bytes(str_from_cstr("const"), f->text + t->start,
                                                                  (long)t->end - t->start)))) { /* spec 031 */
            Token *n = t + 1;
            if ((n->kind == T_IDENT || n->kind == T_TYPE_IDENT) &&
                str_eq_bytes(name, f->text + n->start, (long)n->end - n->start))
                return 1;
        }
    }
    return 0;
}

Str *str_join2_c(Str *a, char *b)
{
    return str_join2(a, str_from_cstr(b));
}

/* The file's id when it declares a top-level item `name`, else -1. A file not loaded yet is
 * read and lexed quietly (its own errors are reported only if it is imported later). */
long path_defines(Str *path, Str *name)
{
    long id = file_find(path);
    if (id < 0) {
        char *text;
        long len = read_whole_file(path->data, &text);
        if (len < 0) return -1;
        id = file_add(path, text, len);
    }
    if (!file_get((int)id)->tokens) {
        diag_mute = 1;
        lex((int)id);
        diag_mute = 0;
    }
    return file_defines((int)id, name) ? id : -1;
}

/* "a/b.vo" + name -> "a.b.name". */
Str *import_text_for(Str *rel, Str *name)
{
    (void)name; /* spec 058: the module, not the item */
    SB *sb = sb_new();
    long n = rel->len - 3; /* without ".vo" */
    for (long i = 0; i < n; i++) sb_byte(sb, rel->data[i] == '/' ? '.' : (unsigned char)rel->data[i]);
    return sb_finish(sb);
}

/* Spec 058: E0204 for a module name `m` used without its import, when exactly one module can be meant:
 * `std.m`, or `m.vo` beside the file. The fix (on the first report only; later ones are notes on it, so
 * applying every first fix adds the line once) inserts the import. Returns 1 when reported. */
int missing_module_import(Str *m, Node *at)
{
    Str *stdp = msg("std/$1.vo", m, 0, 0);
    int in_std = 0;
    for (const StdFile *sf = std_files; sf->path; sf++)
        if (str_eq_cstr(stdp, (char *)sf->path)) in_std = 1;
    FileRec *f = file_get((int)at->file);
    int beside = !ld_app && import_exists(str_join2(f->dir, msg("$1.vo", m, 0, 0)));
    if (!in_std && !beside) return 0;
    if (!mm_names) {
        mm_names = vec_new();
        mm_diags = vec_new();
    }
    for (long i = 0; i < mm_names->n; i++)
        if (str_eq(vec_get(mm_names, i), m) && ((Diag *)vec_get(mm_diags, i))->file == at->file) {
            diag_add_related(vec_get(mm_diags, i), (int)at->file, at->start, at->end, str_from_cstr("Also used here."));
            return 1;
        }
    Str *path = in_std && beside ? 0 : in_std ? msg("std.$1", m, 0, 0) : m;
    Diag *d = path ? ck_err("E0204", at, msg("`$1` is not imported: add `import $2;`.", m, path, 0))
                   : ck_err("E0204", at, msg("`$1` is not imported: it may be `std.$1` or `$1.vo`.", m, 0, 0));
    if (path) {
        long pos = import_insert_at((int)at->file);
        diag_add_fix(d, (int)at->file, pos, pos, msg("import $1;\n", path, 0, 0));
    }
    vec_push(mm_names, m);
    vec_push(mm_diags, d);
    return 1;
}

/* Searches the *.vo files below dir (ending in '/' or empty; rel is the same directory relative to
 * the importing file) for a top-level item `name`, skipping the file `self`. Returns the import
 * text ("a.b.name") or 0. */
Str *scan_dir(Str *dir, Str *rel, Str *name, int self)
{
    long fd = sys_open(dir->len ? dir->data : ".", O_DIRECTORY); /* an empty directory part means "." */
    if (fd < 0) return 0;
    char *buf = alloc(32768);
    Str *found = 0;
    while (!found) {
        long got = sys_getdents64((int)fd, buf, 32768);
        if (got <= 0) break;
        for (long off = 0; off < got && !found;) {
            char *ent = buf + off; /* linux_dirent64: reclen at 16, type at 18, name at 19 */
            off += *(uint16_t *)(ent + 16);
            if (ent[19] == '.') continue;
            Str *ename = str_from_cstr(ent + 19);
            if (ent[18] == 4) { /* DT_DIR: recurse */
                Str *sub = str_join2_c(str_join2(dir, ename), "/");
                Str *subrel = str_join2_c(str_join2(rel, ename), "/");
                found = scan_dir(sub, subrel, name, self);
                continue;
            }
            if (!ends_with_vo(ename)) continue;
            if (!inside_tree(str_join2(dir, ename))) continue; /* spec 013 (M4): never read outside */
            long id = path_defines(str_join2(dir, ename), name);
            if (id < 0 || id == self) continue; /* the importing file itself */
            found = import_text_for(str_join2(rel, ename), name);
        }
    }
    sys_close((int)fd);
    return found;
}

/* The import text that would bring `name` into the file from a .vo file under its directory, or 0. */
Str *find_import(int file, Str *name)
{
    Str *dir = file_get(file)->dir;
    return scan_dir(dir, str_new(0, 0), name, file); /* rel: "" */
}

/* Where a new import line goes: after the last import line, or at the start of the file. */
long import_insert_at(int file)
{
    FileRec *f = file_get(file);
    Vec *items = f->ast->a.v;
    long end = -1; /* end of the last import */
    for (long i = 0; i < items->n; i++)
        if (items->data[i].n->kind == N_IMPORT) end = items->data[i].n->end;
    if (end < 0) return 0;
    long at = end; /* just after the newline that ends that line */
    while (at < f->len)
        if (f->text[at++] == '\n') break;
    return at;
}
