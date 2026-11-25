/* Programs across files: import resolution, loading, cycle detection, binding (contracts/grammar.md
 * §Imports; ported from src/loader.s, spec 008). */
#include "vo.h"

#define FS_NEW 0
#define FS_LOADING 1
#define FS_LOADED 2
#define FS_CHECKED 3
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

/* The imported file relative to the importer's directory: "a/b.vo" for `import a.b.name;`. */
Str *import_rel(Node *import)
{
    SB *sb = sb_new();
    for (long i = 0; i < import->a.v->n - 1; i++) {
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
        if (imp->a.v->n < 2) {
            ck_err("E0205", imp, str_from_cstr("An import needs a file and a name: `import file.name;`."));
            continue;
        }
        Str *rel = import_rel(imp);
        Str *path = import_path(imp, rel, f); /* spec 007: std/ and package roots */
        if ((void *)path == NO_PACKAGE) continue; /* E0701/E0702, reported */
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
    SB *sb = sb_new();
    long n = rel->len - 3; /* without ".vo" */
    for (long i = 0; i < n; i++) sb_byte(sb, rel->data[i] == '/' ? '.' : (unsigned char)rel->data[i]);
    sb_byte(sb, '.');
    sb_str(sb, name);
    return sb_finish(sb);
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
