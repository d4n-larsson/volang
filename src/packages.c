/* Packages (spec 007 US3): the vo.packages manifest, content hashes, import resolution and
 * `vo package` (research R8, contracts/packages.md; ported from src/packages.s, spec 008). */
#include "vo.h"

/* A manifest entry. */
typedef struct Pkg {
    Str *name;
    Str *dir; /* the directory, absolute ("" is /) */
    Str *raw; /* the directory as written in the manifest */
    char pin[32];
    long state; /* 0 not checked yet, 1 verified, 2 failed (reported) */
    long start; /* the line's span in the manifest */
    long end;
} Pkg;
CHECK_SIZE(Pkg, 80);

#define NO_PACKAGE ((void *)-1)
#define M_MISSING "Package $1: the directory $2 is missing or has no .vo files."
#define M_UNREADABLE "Package $1: the directory $2 has a directory that cannot be read, or a link leading outside it."
#define MANIFEST "/vo.packages"

static long pkg_searched; /* 1 once the manifest was looked for */
long pkg_manifest;        /* its file id, or -1 */
Str *pkg_dir;             /* its directory (or where it would be) */
Vec *pkg_entries;         /* entries */
static char *pkg_tok_end;
static char pkg_hash_buf[32];

/* ---- the manifest ---- */

/* Looks for the nearest vo.packages from dir up, once, and parses it (E0703, E0704). */
void pkg_load(Str *dir)
{
    if (pkg_searched) return;
    pkg_searched = 1;
    pkg_entries = vec_new();
    if (ld_app) { /* spec 034: the manifest the build embedded; its directory is the path's */
        pkg_manifest = app_manifest;
        if (pkg_manifest >= 0) {
            Str *p = file_get((int)pkg_manifest)->path;
            pkg_dir = str_new(p->data, p->len - (long)sizeof MANIFEST + 1);
            pkg_parse(pkg_manifest);
        }
        return;
    }
    pkg_manifest = find_up_dir(dir, MANIFEST, (void **)&pkg_dir);
    if (pkg_manifest >= 0) pkg_parse(pkg_manifest);
}

static int pkg_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* The next space-separated token at or after p (p == end: none); pkg_tok_end gets its end. */
char *pkg_token(char *p, char *end)
{
    while (p < end && pkg_space((unsigned char)*p)) p++;
    char *q = p;
    while (q < end && !pkg_space((unsigned char)*q)) q++;
    pkg_tok_end = q;
    return p;
}

/* 1 when p[0..n) is an identifier other than `std`. */
long pkg_name_ok(char *p, long n)
{
    if (n <= 0) return 0;
    for (long i = 0; i < n; i++) {
        int c = (unsigned char)p[i];
        int lower = c | 32; /* a letter, either case */
        if (c == '_' || (lower >= 'a' && lower <= 'z')) continue;
        if (!i || c < '0' || c > '9') return 0; /* digits only after the first character */
    }
    return !(n == 3 && p[0] == 's' && p[1] == 't' && p[2] == 'd');
}

static int hex_nibble(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 87;
    return -1;
}

/* 64 lowercase hex digits to 32 bytes; 1 when they are, else 0. */
long pkg_hex_decode(char *p, char *out)
{
    for (long i = 0; i < 64; i += 2) {
        int hi = hex_nibble((unsigned char)p[i]);
        if (hi < 0) return 0;
        int lo = hex_nibble((unsigned char)p[i + 1]);
        if (lo < 0) return 0;
        out[i / 2] = (char)(hi << 4 | lo);
    }
    return 1;
}

/* One manifest line [line, le): an entry, or E0703 / E0704 on the line. */
static void pkg_parse_line(long file, char *text, char *line, char *le)
{
    char *name = pkg_token(line, le); /* first token: blank or a comment line is nothing */
    if (name == le || *name == '#') return;
    char *name_end = pkg_tok_end;
    char *code = "E0703";
    Str *m;
    for (char *c = name; c < le; c++) /* spec 013 (L3): no control bytes (NUL, escapes) in an entry */
        if (((unsigned char)*c < 0x20 && *c != '\t' && *c != '\r') || *c == 0x7f) goto bad;
    if (!pkg_name_ok(name, name_end - name)) goto bad;
    char *dir = pkg_token(name_end, le); /* DIRECTORY */
    if (dir == le) goto bad;
    char *dir_end = pkg_tok_end;
    char *hash = pkg_token(dir_end, le); /* sha256:HEX */
    char *hash_end = pkg_tok_end;
    if (hash_end - hash != 71) goto bad;
    if (pkg_token(hash_end, le) != le) goto bad; /* nothing after it */
    if (!doc_starts(hash, "sha256:")) goto bad;
    Pkg *e = alloc(sizeof(Pkg));
    if (!pkg_hex_decode(hash + 7, e->pin)) goto bad;
    e->name = str_new(name, name_end - name);
    if (pkg_find(e->name)) { /* a second entry of the name: E0704 */
        m = msg("Package $1 is listed twice.", e->name, 0, 0);
        code = "E0704";
        goto report;
    }
    e->raw = str_new(dir, dir_end - dir);
    e->dir = e->raw->data[0] == '/' ? e->raw : path_join(pkg_dir, e->raw); /* or under the manifest's directory */
    e->state = 0;
    e->start = line - text;
    e->end = le - text;
    vec_push(pkg_entries, e);
    return;
bad:
    m = str_from_cstr("Write a package as `NAME DIRECTORY sha256:HEX` (NAME an identifier other than std, 64 "
                      "lowercase hex digits).");
report:
    diag_add(code, SEV_ERROR, (int)file, line - text, le - text, m);
}

/* Parses the manifest into pkg_entries; bad lines are E0703, a second entry of a name E0704. */
void pkg_parse(long file)
{
    FileRec *f = file_get((int)file);
    char *text = f->text, *end = text + f->len;
    for (char *line = text; line < end;) {
        char *le = line;
        while (le < end && *le != '\n') le++;
        pkg_parse_line(file, text, line, le);
        line = le + 1;
    }
}

/* dir + "/" + rel ("" is the root). */
Str *path_join(Str *dir, Str *rel)
{
    SB *sb = sb_new();
    sb_str(sb, dir);
    sb_byte(sb, '/');
    sb_str(sb, rel);
    return sb_finish(sb);
}

/* The entry listed under name, or 0. */
void *pkg_find(Str *name)
{
    Vec *v = pkg_entries;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++) {
        Pkg *e = vec_get(v, i);
        if (str_eq(e->name, name)) return e;
    }
    return 0;
}

/* ---- the content hash ---- */

static Vec *pkg_files; /* spec 013 (H6): {path, bytes} of every hashed file: what is loaded */

/* The bytes hashed for path (their length), -2 when path lies in a listed package but was not hashed
 * (H5: never loaded from the disk), or -1 when it is no package file. */
long pkg_cached(Str *path, char **text)
{
    for (long i = 0; pkg_files && i + 1 < pkg_files->n; i += 2)
        if (str_eq(vec_get(pkg_files, i), path)) {
            Str *t = vec_get(pkg_files, i + 1);
            *text = t->data;
            return t->len;
        }
    for (long i = 0; pkg_entries && i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        if (e->state == 1 && path_under(path, e->dir)) return -2;
    }
    return -1;
}

/* Spec 013 (M4): 1 when the resolved path lies in a listed package's directory (resolved). */
int pkg_contains(Str *real)
{
    for (long i = 0; pkg_entries && i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        Str *dir = rt_resolve(e->dir);
        if (dir && path_under(real, dir)) return 1;
    }
    return 0;
}

/* SHA-256 over every .vo file under dir, in byte order of the relative paths:
 * PATH 0 LENGTH 0 BYTES each. 0 when hashed (keep: the bytes are kept for loading); -1 when dir is
 * missing or has no .vo file; -2 when a directory cannot be read or a link leads outside it (H5). */
long pkg_hash(Str *dir, char *out, int keep)
{
    long fd = sys_open(dir->data, O_DIRECTORY);
    if (fd < 0) return -1;
    long from = path_n; /* the files, appended to path_list and taken back after */
    walk_strict = 1;
    walk_failed = 0;
    walk_root = rt_resolve(dir);
    if (!walk_root) walk_failed = 1;
    else walk_dir(dir, (int)fd);
    walk_strict = 0;
    sys_close((int)fd);
    sort_paths(from);
    long r = walk_failed ? -2 : -1;
    if (keep && !pkg_files) pkg_files = vec_new();
    if (!walk_failed && path_n != from) {
        sha256_init();
        long i;
        for (i = from; i < path_n; i++) {
            Str *p = ((Str **)path_list)[i]; /* DIR/REL */
            long skip = dir->len + 1;
            sha256_update(p->data + skip, p->len - skip);
            pkg_hash_buf[0] = 0; /* NUL */
            sha256_update(pkg_hash_buf, 1);
            char *text;
            long n = read_whole_file(p->data, &text);
            if (n < 0) break; /* a file that cannot be read */
            SB *sb = sb_new();
            int_to_dec(sb, n);
            sha256_update(sb->ptr, sb->len);
            pkg_hash_buf[0] = 0;
            sha256_update(pkg_hash_buf, 1);
            sha256_update(text, n);
            if (keep) {
                vec_push(pkg_files, p);
                vec_push(pkg_files, str_new(text, n));
            }
        }
        if (i == path_n) {
            sha256_final(out);
            r = 0;
        }
    }
    path_n = from;
    return r;
}

/* "sha256:" and 64 lowercase hex digits. */
Str *pkg_hash_text(char *hash)
{
    static const char hexchars[] = "0123456789abcdef";
    SB *sb = sb_new();
    sb_cstr(sb, "sha256:");
    for (int i = 0; i < 32; i++) {
        int b = (unsigned char)hash[i];
        sb_byte(sb, hexchars[b >> 4]);
        sb_byte(sb, hexchars[b & 15]);
    }
    return sb_finish(sb);
}

/* Hashes the package once: 1 when it matches its pin, else 0 (E0701 or E0702 at node, once). */
long pkg_verify(void *entry, Node *node)
{
    Pkg *e = entry;
    if (e->state) return e->state != 2;
    if (ld_app) return e->state = 1; /* spec 034: verified by the build; the payload's hash covers the bytes */
    char *code;
    Str *m;
    long h = pkg_hash(e->dir, pkg_hash_buf, 1);
    if (h < 0) {
        m = msg(h == -2 ? M_UNREADABLE : M_MISSING, e->name, e->raw, 0);
        code = "E0702";
    } else if (mem_eq(pkg_hash_buf, e->pin, 32)) {
        e->state = 1;
        return 1;
    } else { /* E0701 with both hashes */
        Str *pinned = pkg_hash_text(e->pin);
        Str *now = pkg_hash_text(pkg_hash_buf);
        Str *changed = msg("Package $1 changed: the manifest pins $2, its files hash to $3.", e->name, pinned, now);
        SB *sb = sb_new(); /* the command that pins the package again: ENTRY NAME DIRECTORY */
        sb_str(sb, file_get(0)->path);
        sb_byte(sb, ' ');
        sb_str(sb, e->name);
        sb_byte(sb, ' ');
        /* spec 013 (L5): the directory as the command will read it: from the working directory */
        Str *cwd = rt_start_cwd(), *real = rt_resolve(e->dir);
        sb_str(sb, e->raw->data[0] == '/' || !cwd || !real ? e->raw : pkg_rel_dir(cwd, real));
        m = msg("$1 Review the change, then run `vo package $2`.", changed, sb_finish(sb), 0);
        code = "E0701";
    }
    ck_err(code, node, m);
    e->state = 2;
    return 0;
}

/* ---- imports ---- */

/* For `import NAME.a.b.item;` with NAME listed: the file DIRECTORY/a/b.vo, after the package's
 * hash is checked; -1 when the package failed its check (reported once); 0 when NAME is no package. */
void *pkg_import_path(Node *import, FileRec *importer)
{
    Str *start = rt_resolve(importer->dir); /* the manifest: from the first importer (the entry) up */
    if (!start) start = rt_start_cwd();
    pkg_load(start);
    Vec *segs = import->a.v; /* NAME.a.b.item: at least three segments */
    if (segs->n < 3) return 0;
    Pkg *e = pkg_find(vec_get(segs, 0));
    if (!e) return 0;
    SB *sb = sb_new(); /* E0705: NAME is also a directory next to the importer */
    sb_str(sb, importer->dir);
    sb_str(sb, e->name);
    long fd = ld_app ? -1 : sys_open(sb_finish(sb)->data, O_DIRECTORY); /* (spec 034: checked by the build) */
    if (fd >= 0) {
        sys_close((int)fd);
        ck_err("E0705", import,
               msg("`$1` is both a listed package and a directory next to this file: rename one of them.", e->name,
                   0, 0));
    }
    if (!pkg_verify(e, import)) return NO_PACKAGE;
    SB *path = sb_new(); /* DIRECTORY/a/b.vo */
    sb_str(path, e->dir);
    for (long i = 1; i < import->a.v->n - 1; i++) {
        Str *seg = vec_get(import->a.v, i);
        sb_byte(path, '/');
        sb_str(path, seg);
    }
    sb_cstr(path, ".vo");
    return sb_finish(path);
}

/* "@NAME/relative/path.vo" when the resolved path lies in a listed package, else 0. */
Str *pkg_relpath(Str *path)
{
    Vec *v = pkg_entries;
    if (!v) return 0;
    for (long i = 0; i < v->n; i++) {
        Pkg *e = vec_get(v, i);
        if (!path_under(path, e->dir)) continue;
        SB *sb = sb_new();
        sb_byte(sb, '@');
        sb_str(sb, e->name);
        long k = e->dir->len; /* the rest of the path, from its "/" */
        sb_bytes(sb, path->data + k, path->len - k);
        return sb_finish(sb);
    }
    return 0;
}

/* ---- vo doc (spec 007 T044, research R7) ---- */

/* The entry listed as name in the nearest manifest from the working directory, with its hash
 * checked (E0701/E0702 at its manifest line; exit 65 when it fails); 0 when name is not listed. */
void *pkg_for_doc(Str *name)
{
    pkg_load(rt_start_cwd());
    Pkg *e = pkg_find(name);
    if (!e) return 0;
    Node at = {0}; /* the manifest line, for the report */
    at.file = (uint32_t)pkg_manifest;
    at.start = (uint32_t)e->start;
    at.end = (uint32_t)e->end;
    if (!pkg_verify(e, &at)) vo_exit(finish_check(1));
    return e;
}

/* The entry's name. */
Str *pkg_entry_name(void *entry)
{
    return ((Pkg *)entry)->name;
}

/* DIRECTORY/a/b.vo for the module path "a.b" under entry, or 0 when there is no such file. */
Str *pkg_doc_path(void *entry, Str *rel)
{
    SB *sb = sb_new();
    sb_str(sb, ((Pkg *)entry)->dir);
    sb_byte(sb, '/');
    for (long i = 0; i < rel->len; i++) sb_byte(sb, rel->data[i] == '.' ? '/' : rel->data[i]);
    sb_cstr(sb, ".vo");
    Str *path = sb_finish(sb);
    long fd = sys_open(path->data, O_RDONLY);
    if (fd < 0) return 0;
    sys_close((int)fd);
    return path;
}

/* The entry's module names ("money.format", "money.rates.eur"), in byte order of their paths. */
Vec *pkg_doc_modules(void *entry)
{
    Pkg *e = entry;
    Vec *out = vec_new();
    long fd = sys_open(e->dir->data, O_DIRECTORY);
    if (fd < 0) return out;
    long from = path_n; /* the files, appended to path_list and taken back after */
    walk_dir(e->dir, (int)fd);
    sys_close((int)fd);
    sort_paths(from);
    for (long i = from; i < path_n; i++) {
        Str *p = ((Str **)path_list)[i]; /* DIR/a/b.vo */
        SB *sb = sb_new();
        sb_str(sb, e->name);
        sb_byte(sb, '.');
        for (long k = e->dir->len + 1; k < p->len - 3; k++) sb_byte(sb, p->data[k] == '/' ? '.' : p->data[k]);
        vec_push(out, sb_finish(sb));
    }
    path_n = from;
    return out;
}

/* ---- vo package ---- */

/* to relative to the directory from (both absolute, "" is /): "shared/money", "../shared/money". */
Str *pkg_rel_dir(Str *from, Str *to)
{
    char *f = from->data, *t = to->data;
    long fl = from->len, tl = to->len;
    long i = 0, b = 0; /* b: the last common component boundary */
    for (; i < fl && i < tl; i++) {
        if (f[i] != t[i]) break;
        if (f[i] == '/') b = i;
    }
    if (i >= fl || i >= tl) {
        if (i == fl) { /* from ends here: to continues with "/" or ends */
            if (i == tl || t[i] == '/') b = i;
        } else if (f[i] == '/') { /* to ends here and from continues with "/" */
            b = i;
        }
    }
    SB *sb = sb_new();
    for (long k = b; k < from->len; k++) /* one "../" per component of from after the boundary */
        if (f[k] == '/') sb_cstr(sb, "../");
    if (b + 1 < to->len) /* then to after the boundary */
        sb_bytes(sb, t + b + 1, to->len - (b + 1));
    else if (sb->len) /* no rest: drop the last "/", or "." for the same directory */
        sb->len--;
    else
        sb_byte(sb, '.');
    return sb_finish(sb);
}

/* Insertion sort of pkg_entries by name. */
void pkg_sort(void)
{
    W *d = pkg_entries->data;
    long n = pkg_entries->n;
    for (long i = 1; i < n; i++) {
        for (long j = i; j > 0; j--) {
            Pkg *a = d[j - 1].p, *b = d[j].p;
            if (str_cmp(a->name, b->name) <= 0) break;
            d[j - 1].p = b;
            d[j].p = a;
        }
    }
}

/* Writes the manifest: the header comment, then one line per entry in name order. */
void pkg_write(void)
{
    pkg_sort();
    SB *sb = sb_new();
    sb_cstr(sb, "# vo packages: update with `vo package ENTRY NAME DIRECTORY`\n");
    for (long i = 0; i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        sb_str(sb, e->name);
        sb_byte(sb, ' ');
        sb_str(sb, e->raw);
        sb_byte(sb, ' ');
        sb_str(sb, pkg_hash_text(e->pin));
        sb_byte(sb, '\n');
    }
    SB *p = sb_new();
    sb_str(p, pkg_dir);
    sb_cstr(p, MANIFEST);
    Str *path = sb_finish(p);
    write_file_safely(path, sb_finish(sb));
}

/* `added NAME DIR HASH`, `updated NAME: OLD -> NEW`, `unchanged NAME`, `removed NAME`; or JSON. */
void pkg_report(char *change, void *entry, Str *old_hash_text)
{
    Pkg *e = entry;
    Str *now = pkg_hash_text(e->pin); /* the hash now */
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"package\",\"name\":");
        json_str_into(sb, e->name);
        sb_cstr(sb, ",\"change\":\"");
        sb_cstr(sb, change);
        sb_cstr(sb, "\",\"directory\":");
        json_str_into(sb, e->raw);
        sb_cstr(sb, ",\"hash\":\"");
        sb_str(sb, now);
        if (old_hash_text) {
            sb_cstr(sb, "\",\"old_hash\":\"");
            sb_str(sb, old_hash_text);
        }
        sb_cstr(sb, "\"}\n");
    } else {
        if (cstr_eq(change, "added")) {
            sb_cstr(sb, "added ");
            sb_str(sb, e->name);
            sb_byte(sb, ' ');
            sb_str(sb, e->raw);
            sb_byte(sb, ' ');
            sb_str(sb, now);
        } else if (cstr_eq(change, "updated")) {
            sb_cstr(sb, "updated ");
            sb_str(sb, e->name);
            sb_cstr(sb, ": ");
            sb_str(sb, old_hash_text);
            sb_cstr(sb, " -> ");
            sb_str(sb, now);
        } else { /* unchanged NAME / removed NAME */
            sb_cstr(sb, cstr_eq(change, "removed") ? "removed " : "unchanged ");
            sb_str(sb, e->name);
        }
        sb_byte(sb, '\n');
    }
    out_bytes(1, sb->ptr, sb->len);
}

/* A package error outside any source line (vo package): reported, exit 65. */
void pkg_fail(char *code, Str *message)
{
    toolchain_error((int)opt_json, (int)report_fd, code, message, 0);
    vo_exit(EXIT_CHECK);
}

/* `vo package [--json] ENTRY NAME DIRECTORY` and `vo package --remove ENTRY NAME`. */
int cmd_package(void)
{
    if (pos_n != 3 - opt_remove) /* ENTRY NAME [DIRECTORY] */
        usage_error(str_from_cstr("`package` takes ENTRY NAME DIRECTORY, or --remove ENTRY NAME."));
    char **args = (char **)pos_args;
    if (!pkg_name_ok(args[1], cstr_len(args[1])))
        usage_error_arg("A package name is an identifier other than std: `$1` is not.", args[1]);
    Str *name = str_from_cstr(args[1]);
    Str *entry = str_from_cstr(args[0]); /* ENTRY's directory: its path up to the last "/" */
    long k = entry->len;
    while (k && entry->data[k - 1] != '/') k--;
    Str *start = rt_resolve(str_new(entry->data, k));
    if (!start) start = rt_start_cwd();
    pkg_load(start);
    if (diag_errors) { /* a malformed manifest: report it, change nothing */
        diag_emit_all((int)opt_json, (int)report_fd);
        vo_exit(EXIT_CHECK);
    }
    Pkg *e = pkg_find(name); /* the entry, or 0 */
    if (opt_remove) {
        if (!e) usage_error_arg("No package `$1` is listed in the manifest.", args[1]);
        Vec *v = pkg_entries; /* take it out (swap with the last; pkg_write sorts) */
        long i = 0;
        while (v->data[i].p != e) i++;
        v->n--;
        v->data[i] = v->data[v->n];
        pkg_write();
        pkg_report("removed", e, 0);
        return 0;
    }
    Str *dir_arg = str_from_cstr(args[2]); /* DIRECTORY: resolved, hashed */
    Str *abs = rt_resolve(dir_arg);
    if (!abs) pkg_fail("E0702", msg("Cannot find the directory `$1`.", dir_arg, 0, 0));
    long h = pkg_hash(abs, pkg_hash_buf, 0);
    if (h < 0) pkg_fail("E0702", msg(h == -2 ? M_UNREADABLE : M_MISSING, name, str_from_cstr(args[2]), 0));
    Str *raw = pkg_rel_dir(pkg_dir, abs); /* the directory as the manifest writes it */
    if (!e) {                             /* added */
        e = alloc(sizeof(Pkg));
        e->name = name;
        e->dir = abs;
        e->raw = raw;
        mem_copy(e->pin, pkg_hash_buf, 32);
        vec_push(pkg_entries, e);
        pkg_write();
        pkg_report("added", e, 0);
        return 0;
    }
    if (mem_eq(e->pin, pkg_hash_buf, 32) && str_eq(e->raw, raw)) { /* unchanged: same hash and directory */
        pkg_report("unchanged", e, 0);
        return 0;
    }
    Str *old = pkg_hash_text(e->pin); /* updated */
    e->raw = raw;
    e->dir = abs;
    mem_copy(e->pin, pkg_hash_buf, 32);
    pkg_write();
    pkg_report("updated", e, old);
    return 0;
}
