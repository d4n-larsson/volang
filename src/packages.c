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
    Str *url; /* spec 041: where it was fetched from (with its #PATH), or 0; spec 061: a versioned one's source */
    long versioned; /* spec 061: the fourth field is SOURCE@MAJOR.MINOR.PATCH */
    long ver[3];
    long indirect; /* spec 061: the fifth field: here only because another package requires it */
} Pkg;
CHECK_SIZE(Pkg, 128);

#define NO_PACKAGE ((void *)-1)
#define M_MISSING "Package `$1`: the directory `$2` is missing or has no .vo files."
#define M_UNREADABLE "Package `$1`: the directory `$2` has a directory that cannot be read, or a link leading outside it."
#define MANIFEST "/vo.packages"

static long pkg_searched; /* 1 once the manifest was looked for */
long pkg_manifest;        /* its file id, or -1 */
Str *pkg_dir;             /* its directory (or where it would be) */
Vec *pkg_entries;         /* entries */
static char *pkg_tok_end;
static char pkg_hash_buf[32];

/* ---- the manifest ---- */

static Str *pkg_entry;   /* the file whose import made the manifest load: the program's entry, for messages */
static long pkg_command; /* `vo package` runs: what the manifest lacks is its to repair, not an error */
static void pkg_requirements(void);

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
    if (pkg_manifest >= 0 && !pkg_command) pkg_requirements();
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

/* One manifest line [line, le): its entry (name, raw directory, pin, URL, version, `indirect`), or 0 with
 * *problem 0 (blank or a comment), 1 (malformed) or 2 (a malformed version field). No side effects, so a
 * package's own manifest is read with it too (spec 061). */
static Pkg *pkg_line(char *line, char *le, int *problem)
{
    *problem = 0;
    char *name = pkg_token(line, le); /* first token: blank or a comment line is nothing */
    if (name == le || *name == '#') return 0;
    char *name_end = pkg_tok_end;
    *problem = 1;
    for (char *c = name; c < le; c++) /* spec 013 (L3): no control bytes (NUL, escapes) in an entry */
        if (((unsigned char)*c < 0x20 && *c != '\t' && *c != '\r') || *c == 0x7f) return 0;
    if (!pkg_name_ok(name, name_end - name)) return 0;
    char *dir = pkg_token(name_end, le); /* DIRECTORY */
    if (dir == le) return 0;
    char *dir_end = pkg_tok_end;
    char *hash = pkg_token(dir_end, le); /* sha256:HEX */
    char *hash_end = pkg_tok_end;
    if (hash_end - hash != 71) return 0;
    char *url = pkg_token(hash_end, le); /* spec 041: optionally the URL it was fetched from */
    char *url_end = url == le ? le : pkg_tok_end;
    if (url != le && (url_end - url < 9 || !mem_eq(url, "https://", 8))) return 0;
    if (!doc_starts(hash, "sha256:")) return 0;
    Pkg *e = alloc(sizeof(Pkg));
    char *ver = url_end; /* spec 061: SOURCE@MAJOR.MINOR.PATCH */
    while (ver > url && ver[-1] != '@' && ver[-1] != '/') ver--;
    if (url != le && ver > url + 9 && ver[-1] == '@') { /* an `@` in the last part is a version's */
        *problem = 2;
        if (*ver == 'v' || !git_version_parse(ver, url_end - ver, e->ver)) return 0;
        *problem = 1;
        e->versioned = 1;
        url_end = ver - 1;
    }
    char *more = url == le ? le : pkg_token(pkg_tok_end, le); /* spec 061: `indirect`, on a versioned entry */
    if (more != le) {
        int word = pkg_tok_end - more == 8 && mem_eq(more, "indirect", 8);
        if (!e->versioned && !word) return 0; /* as before spec 061: something after an archive's URL */
        *problem = 2;
        if (!e->versioned || !word || pkg_token(pkg_tok_end, le) != le) return 0;
        *problem = 1;
        e->indirect = 1;
    }
    e->url = url == le ? 0 : str_new(url, url_end - url);
    if (!pkg_hex_decode(hash + 7, e->pin)) return 0;
    e->name = str_new(name, name_end - name);
    e->raw = str_new(dir, dir_end - dir);
    *problem = 0;
    return e;
}

/* 1 when dir is the manifest's directory or one above it. A versioned package's directory is the
 * toolchain's to replace and remove (sel_apply), so a manifest must not name the program itself. */
static int pkg_holds_program(Str *dir)
{
    Str *d = rt_resolve(dir), *p = rt_resolve(pkg_dir);
    if (!d || !p) return 0; /* nothing there: nothing to remove */
    if (d->len == 1) return 1; /* "/" */
    return p->len >= d->len && mem_eq(p->data, d->data, d->len) && (p->len == d->len || p->data[d->len] == '/');
}

/* One line of the program's manifest: an entry, or E0703 / E0704 on the line. */
static void pkg_parse_line(long file, char *text, char *line, char *le)
{
    int problem;
    Pkg *e = pkg_line(line, le, &problem);
    char *code = "E0703";
    Str *m;
    if (problem == 1) {
        m = str_from_cstr("Write a package as `NAME DIRECTORY sha256:HEX` (NAME an identifier other than std, 64 "
                          "lowercase hex digits), optionally followed by the https:// URL it was fetched from.");
    } else if (problem) { /* spec 061 */
        m = str_from_cstr("After the hash, write the URL an archive was fetched from, or a versioned package's source as "
                          "`https://HOST/PATH@MAJOR.MINOR.PATCH`, followed by `indirect` or nothing.");
    } else if (!e) {
        return;
    } else if (pkg_find(e->name)) { /* a second entry of the name: E0704 */
        m = msg("Package `$1` is listed twice.", e->name, 0, 0);
        code = "E0704";
    } else if (e->versioned && pkg_holds_program(e->raw->data[0] == '/' ? e->raw : path_join(pkg_dir, e->raw))) {
        m = msg("Package `$1`: the directory `$2` holds this program. A versioned package has a directory of its own.",
                e->name, e->raw, 0);
    } else {
        e->dir = e->raw->data[0] == '/' ? e->raw : path_join(pkg_dir, e->raw); /* or under the manifest's directory */
        e->state = 0;
        e->start = line - text;
        e->end = le - text;
        vec_push(pkg_entries, e);
        return;
    }
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

/* Spec 061 (research R5): what each versioned package's own manifest (the `vo.packages` in its directory)
 * requires must be in the program's manifest, at that version or higher: E0709 at the requirer's line.
 * A program entry without a version for the name is the author's own choice. Nothing is fetched. */
static void pkg_requirements(void)
{
    long n = pkg_entries->n;
    for (long i = 0; i < n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        char *text;
        long len = e->versioned ? read_whole_file(path_join(e->dir, str_from_cstr(MANIFEST))->data, &text) : -1;
        for (char *line = text, *end = text + len; len > 0 && line < end;) {
            char *le = line;
            while (le < end && *le != '\n') le++;
            int problem;
            Pkg *r = pkg_line(line, le, &problem), *have = r && r->versioned ? pkg_find(r->name) : 0;
            int low = 0;
            for (int k = 0; have && have->versioned && k < 3 && !low; k++) {
                if (have->ver[k] > r->ver[k]) break;
                low = have->ver[k] < r->ver[k];
            }
            if (r && r->versioned && !str_eq(r->name, e->name) && (!have || low))
                diag_add("E0709", SEV_ERROR, (int)pkg_manifest, e->start, e->end,
                         msg("Package `$1` requires $2; the manifest $3`.", e->name, msg("`$1` $2", r->name, pkg_ver_text(r->ver), 0),
                             msg(have ? "has $1: run `vo package --repair $2" : "does not list it: run `vo package --repair $2",
                                 have ? pkg_ver_text(have->ver) : 0, pkg_entry ? pkg_entry : str_from_cstr("ENTRY"), 0)));
            line = le + 1;
        }
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
        Str *changed = msg("Package `$1` changed: the manifest pins $2, its files hash to $3.", e->name, pinned, now);
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
    if (!pkg_searched) pkg_entry = importer->path;
    pkg_load(start);
    Vec *segs = import->a.v; /* NAME.a.b (spec 058), or the 0.3 NAME.a.b.item (N_C 1) */
    long keep = segs->n - (import->c.i ? 1 : 0);
    Pkg *e = pkg_find(vec_get(segs, 0));
    if (!e) return 0;
    if (keep < 2) return 0; /* the package name alone: E0205 from the loader */
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
    for (long i = 1; i < keep; i++) {
        Str *seg = vec_get(import->a.v, i);
        sb_byte(path, '/');
        sb_str(path, seg);
    }
    sb_cstr(path, ".vo");
    return sb_finish(path);
}

/* Spec 058: whether NAME is a listed package for files under the importer's manifest. */
int pkg_is_name(Str *name, FileRec *importer)
{
    Str *start = rt_resolve(importer->dir);
    if (!start) start = rt_start_cwd();
    if (!pkg_searched) pkg_entry = importer->path;
    pkg_load(start);
    return pkg_find(name) != 0;
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

/* `MAJOR.MINOR.PATCH` */
Str *pkg_ver_text(long *v)
{
    SB *sb = sb_new();
    for (int k = 0; k < 3; k++) {
        if (k) sb_byte(sb, '.');
        int_to_dec(sb, v[k]);
    }
    return sb_finish(sb);
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
        if (e->url) {
            sb_byte(sb, ' ');
            sb_str(sb, e->url);
        }
        if (e->versioned) {
            sb_byte(sb, '@');
            sb_str(sb, pkg_ver_text(e->ver));
            if (e->indirect) sb_cstr(sb, " indirect");
        }
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

/* ---- spec 041: packages from a URL ----
 * `vo package ENTRY NAME URL` fetches an archive (fetch.c), unpacks it in memory (inflate.c, untar.c)
 * and only then writes its `.vo` files to a staging directory beside the target; the staging
 * directory is pinned like any other and swapped in by renames, and the manifest is written last, so a
 * failure at any point leaves the project as it was. `--restore` does the same for entries whose
 * directory is missing and accepts the result only at its pinned hash. Nothing else in the toolchain
 * fetches anything. */

#define UNPACK_MAX (64L << 20)

long opt_restore;  /* vo package --restore ENTRY */
long opt_repair;   /* vo package --repair ENTRY (spec 061) */
long opt_outdated; /* vo package --outdated ENTRY (spec 061) */
char *opt_pkg_dir; /* vo package --dir DIRECTORY */

typedef struct Dirent64 {
    uint64_t ino;
    int64_t off;
    uint16_t reclen;
    uint8_t type;
    char name[1];
} Dirent64;

static int is_dir(Str *path)
{
    long fd = sys_open(path->data, O_DIRECTORY | O_NOFOLLOW);
    if (fd >= 0) sys_close((int)fd);
    return fd >= 0;
}

/* Creates path and its missing parents (also spec 054's test recordings). */
void mkdir_p(Str *path)
{
    Str *p = str_new(path->data, path->len); /* a copy to cut at each `/` */
    for (long i = 1; i <= p->len; i++) {
        if (i < p->len && p->data[i] != '/') continue;
        char keep = p->data[i];
        p->data[i] = 0;
        vo_syscall3(SYS_mkdirat, AT_FDCWD, (long)p->data, 0755);
        p->data[i] = keep;
    }
}

/* Removes a directory tree this command made or is replacing; links are removed, never followed. */
static void rm_tree(Str *path)
{
    long fd = sys_open(path->data, O_DIRECTORY | O_NOFOLLOW);
    if (fd >= 0) {
        char buf[4096];
        long n;
        while ((n = sys_getdents64((int)fd, buf, sizeof buf)) > 0) {
            for (long at = 0; at < n;) {
                Dirent64 *d = (Dirent64 *)(buf + at);
                at += d->reclen;
                if (d->name[0] == '.' && (!d->name[1] || (d->name[1] == '.' && !d->name[2]))) continue;
                Str *child = path_join(path, str_from_cstr(d->name));
                if (vo_syscall3(SYS_unlinkat, fd, (long)d->name, 0) < 0) rm_tree(child);
            }
        }
        sys_close((int)fd);
    }
    vo_syscall3(SYS_unlinkat, AT_FDCWD, (long)path->data, 0x200 /* AT_REMOVEDIR */);
}

/* Fetches url (with its optional #PATH) and writes the package's `.vo` files under staging. 0 when
 * done; -1 with *why for a failed download or a refused archive (E0706); -2 with *why when the
 * archive holds no `.vo` file where one was asked for (E0702). Nothing is written before the whole
 * archive was read and accepted. */
/* Writes a package's files (TarFile: checked relative paths) under staging: 0, or -1 with *why. */
static int pkg_write_files(Str *staging, Vec *files, Str **why)
{
    mkdir_p(staging);
    for (long i = 0; i < files->n; i++) {
        TarFile *f = vec_get(files, i);
        Str *path = path_join(staging, f->path);
        long cut = path->len;
        while (cut && path->data[cut - 1] != '/') cut--;
        mkdir_p(str_new(path->data, cut - 1));
        long fd = vo_syscall4(SYS_openat, AT_FDCWD, (long)path->data, O_WRCREAT | O_NOFOLLOW | O_CLOEXEC, 0644);
        if (fd < 0) {
            *why = msg("cannot write `$1`", path, 0, 0);
            return -1;
        }
        write_all((int)fd, f->data, f->n);
        sys_close((int)fd);
    }
    return 0;
}

/* The files of a package that count: at least one `.vo` file (a lone vo.packages is no package). */
static int pkg_has_vo(Vec *files)
{
    for (long i = 0; i < files->n; i++) {
        Str *p = ((TarFile *)vec_get(files, i))->path;
        if (p->len >= 4 && mem_eq(p->data + p->len - 3, ".vo", 3)) return 1;
    }
    return 0;
}

/* Spec 061: the package at version ver of the git repository `repo` (folder `inner` or 0) into staging.
 * ver[0] < 0 asks for the highest version and is filled in. 0; -1 with *why (E0706); -2 (E0702: no .vo
 * files there); -3 (E0711: no such version, *why lists the nearest). */
static int pkg_stage_git(Str *repo, Str *inner, long *ver, Str *staging, Str **why)
{
    Str *final = 0;
    Vec *tags = git_versions(repo, &final, why);
    if (!tags) return -1;
    if (!tags->n) {
        *why = msg("$1 has no version tags (`v1.2.3`)", repo, 0, 0);
        return -3;
    }
    long at = 0; /* the first version not below ver */
    GitTag *t = vec_get(tags, tags->n - 1);
    if (ver[0] >= 0) {
        for (t = 0; at < tags->n; at++) {
            GitTag *c = vec_get(tags, at);
            int cmp = 0;
            for (int k = 0; k < 3 && !cmp; k++) cmp = c->v[k] < ver[k] ? -1 : c->v[k] > ver[k];
            if (cmp == 0) t = c;
            if (cmp >= 0) break;
        }
    }
    if (!t) { /* the nearest five: below and above in turn */
        SB *sb = sb_new();
        long lo = at - 1, hi = at;
        for (int k = 0; k < 5 && (lo >= 0 || hi < tags->n); k++) {
            int below = lo >= 0 && (k % 2 == 0 || hi >= tags->n);
            GitTag *c = vec_get(tags, below ? lo-- : hi++);
            if (k) sb_cstr(sb, ", ");
            sb_str(sb, pkg_ver_text(c->v));
        }
        *why = msg("$1 has no version $2. Nearest: $3", repo, pkg_ver_text(ver), sb_finish(sb));
        return -3;
    }
    for (int k = 0; k < 3; k++) ver[k] = t->v[k];
    Vec *files = git_fetch(final, t->id, inner, why);
    if (!files) return -1;
    int r = 0;
    if (!pkg_has_vo(files)) {
        *why = inner ? msg("version $1 has no .vo files under `$2`", pkg_ver_text(ver), inner, 0)
                     : msg("version $1 has no .vo files", pkg_ver_text(ver), 0, 0);
        r = -2;
    }
    if (!r) r = pkg_write_files(staging, files, why);
    git_release();
    return r;
}

static int pkg_stage(Str *url, long *ver, Str *staging, Str **why)
{
    long hash = 0;
    while (hash < url->len && url->data[hash] != '#') hash++;
    Str *inner = hash < url->len ? str_new(url->data + hash + 1, url->len - hash - 1) : 0;
    if (ver) return pkg_stage_git(str_new(url->data, hash), inner, ver, staging, why);
    Str *body = fetch_url(str_new(url->data, hash), why);
    if (!body) return -1;
    char *buf = body->data, *mapped = 0, *bad = 0;
    long n = body->len, cap = 0;
    if (gzip_is(buf, n)) {
        if (gzip_unpack(body->data, body->len, &mapped, &n, &cap, UNPACK_MAX, &bad)) {
            *why = str_from_cstr(bad);
            return -1;
        }
        buf = mapped;
    } else if (n > UNPACK_MAX) {
        *why = str_from_cstr("it unpacks to more than 64 MiB");
        return -1;
    }
    int r = 0;
    Vec *all = untar(buf, n, &bad);
    Vec *files = all ? untar_select(all, inner) : 0;
    if (!all) {
        *why = str_from_cstr(bad);
        r = -1;
    } else if (!files->n) {
        *why = inner ? msg("the archive has no .vo files under `$1`", inner, 0, 0) : str_from_cstr("the archive has no .vo files");
        r = -2;
    }
    if (!r) r = pkg_write_files(staging, files, why);
    pk_unmap(mapped, cap);
    return r;
}

/* ---- what a package declares (research R7) ---- */

static const char *const effect_order[] = {"console", "input",  "file.read", "file.write", "file.delete", "net",
                                           "clock",   "random", "env",       "serve",      "process",     "entropy", "log", 0};

/* The place of an effect text (`net("a")`) in the canonical order, or -1 for an effect parameter. */
static long effect_rank(Str *e)
{
    long n = 0;
    while (n < e->len && e->data[n] != '(') n++;
    for (long i = 0; effect_order[i]; i++)
        if (cstr_len((char *)effect_order[i]) == n && mem_eq(e->data, (char *)effect_order[i], n)) return i;
    return -1;
}

/* What `vo doc MODULE --json` prints, from a child (a package that does not check ends the child, not
 * this command); *status gets its exit status. */
static Str *doc_capture(Str *module, int *status)
{
    int fds[2];
    *status = 1;
    if (sys_pipe(fds) < 0) return str_from_cstr("");
    long pid = sys_fork();
    if (pid == 0) {
        sys_close(fds[0]);
        sys_dup_to(fds[1], 1);
        sys_dup_to(fds[1], 2);
        long dir = sys_open(pkg_dir->len ? pkg_dir->data : "/", O_DIRECTORY);
        if (dir >= 0) vo_syscall1(SYS_fchdir, dir); /* the manifest's directory, wherever vo was started */
        static char *arg[1];
        arg[0] = module->data;
        pos_args = (long)arg;
        pos_n = 1;
        opt_json = 1;
        report_fd = 1;
        vo_exit(cmd_doc());
    }
    sys_close(fds[1]);
    SB *sb = sb_new();
    char buf[4096];
    long n;
    while ((n = sys_read(fds[0], buf, sizeof buf)) > 0 || n == -4 /* EINTR */)
        if (n > 0) sb_bytes(sb, buf, n);
    sys_close(fds[0]);
    int st = 0;
    if (pid > 0) sys_wait(pid, &st);
    *status = pid > 0 && !(st & 0x7f) ? (st >> 8) & 0xff : 1;
    return sb_finish(sb);
}

/* The union of the effects the package's exported functions declare, as texts in canonical order;
 * 0 with *note (the first error) when the package cannot be read for them. */
static Vec *pkg_effects(Pkg *e, Str **note)
{
    Vec *out = vec_new();
    Vec *modules = pkg_doc_modules(e);
    *note = 0;
    for (long m = 0; m < modules->n; m++) {
        int status;
        Str *text = doc_capture(vec_get(modules, m), &status);
        for (long at = 0; at < text->len;) {
            long end = at;
            while (end < text->len && text->data[end] != '\n') end++;
            Str *line = str_new(text->data + at, end - at); /* NUL-terminated: the reader's form */
            at = end + 1;
            if (!line->len || line->data[0] != '{' || !js_skip(line->data, 0)) continue;
            char *kind = js_get(line->data, "kind");
            if (kind && !js_is(kind, "doc", 3)) { /* a diagnostic or a toolchain error: the first one is the note */
                char *message = js_get(line->data, "message");
                if (message && !js_is(kind, "summary", 7) && !*note) {
                    Str *msg_text = str_new(line->data, line->len); /* room for the unescaped text */
                    msg_text->len = js_unescape(message, msg_text->data);
                    *note = msg_text;
                }
                continue;
            }
            char *eff = js_get(line->data, "effects");
            if (!eff || *eff != '[') continue;
            for (char *p = js_ws(eff + 1); *p == '"'; p = js_ws(p)) {
                Str *one = str_new(line->data, line->len);
                one->len = js_unescape(p, one->data);
                p = js_ws(js_skip(p, 0));
                if (*p == ',') p++;
                long rank = effect_rank(one), k = 0;
                if (rank < 0) continue; /* an effect parameter: the caller's, not the package's */
                for (; k < out->n; k++) { /* insert in order, once */
                    Str *have = vec_get(out, k);
                    long hr = effect_rank(have);
                    if (hr > rank || (hr == rank && str_cmp(have, one) >= 0)) break;
                }
                if (k < out->n && str_eq(vec_get(out, k), one)) continue;
                vec_push(out, one);
                for (long j = out->n - 1; j > k; j--) out->data[j] = out->data[j - 1];
                out->data[k].p = one;
            }
        }
        if (status && !*note) *note = str_from_cstr("its documentation could not be produced");
        if (status) return 0;
    }
    return out;
}

/* The members of a that are not in b. */
static Vec *set_minus(Vec *a, Vec *b)
{
    Vec *out = vec_new();
    for (long i = 0; a && i < a->n; i++) {
        int found = 0;
        for (long k = 0; b && k < b->n && !found; k++) found = str_eq(vec_get(a, i), vec_get(b, k));
        if (!found) vec_push(out, vec_get(a, i));
    }
    return out;
}

static void list_text(SB *sb, char *label, Vec *v)
{
    sb_cstr(sb, label);
    for (long i = 0; i < v->n; i++) {
        if (i) sb_cstr(sb, ", ");
        sb_str(sb, vec_get(v, i));
    }
    if (!v->n) sb_cstr(sb, "none");
    sb_byte(sb, '\n');
}

static void list_json(SB *sb, char *key, Vec *v)
{
    sb_cstr(sb, ",\"");
    sb_cstr(sb, key);
    sb_cstr(sb, "\":[");
    for (long i = 0; i < v->n; i++) {
        if (i) sb_byte(sb, ',');
        json_str_into(sb, vec_get(v, i));
    }
    sb_byte(sb, ']');
}

/* The report of a fetched package: the local form's line or record, with where it came from, what it
 * declares, and on an update what that gained and lost. */
static Str *old_ver; /* spec 061: the version an updated entry had, for the report */
static Vec *rep_by;   /* spec 061: the packages that require the reported one (an indirect entry), or 0 */

static Str *rep_at;   /* the version they require, for a `local` entry */

static void by_text(SB *sb)
{
    sb_cstr(sb, " (required by ");
    for (long i = 0; i < rep_by->n; i++) {
        if (i) sb_cstr(sb, ", ");
        sb_str(sb, vec_get(rep_by, i));
    }
    if (rep_at) {
        sb_cstr(sb, " at ");
        sb_str(sb, rep_at);
    }
    sb_byte(sb, ')');
}

static void pkg_report_fetched(char *change, Pkg *e, Str *old_hash, Vec *before, Vec *now, Str *note)
{
    SB *sb = sb_new();
    Vec *gained = before && now ? set_minus(now, before) : 0, *lost = before && now ? set_minus(before, now) : 0;
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"package\",\"name\":");
        json_str_into(sb, e->name);
        sb_cstr(sb, ",\"change\":\"");
        sb_cstr(sb, change);
        sb_cstr(sb, "\",\"directory\":");
        json_str_into(sb, e->raw);
        sb_cstr(sb, ",\"hash\":\"");
        sb_str(sb, pkg_hash_text(e->pin));
        sb_byte(sb, '"');
        if (old_hash) {
            sb_cstr(sb, ",\"old_hash\":\"");
            sb_str(sb, old_hash);
            sb_byte(sb, '"');
        }
        if (e->url) {
            sb_cstr(sb, e->versioned ? ",\"source\":" : ",\"url\":");
            json_str_into(sb, e->url);
        }
        if (e->versioned) {
            sb_cstr(sb, ",\"version\":\"");
            sb_str(sb, pkg_ver_text(e->ver));
            sb_byte(sb, '"');
            if (old_ver) {
                sb_cstr(sb, ",\"old_version\":\"");
                sb_str(sb, old_ver);
                sb_byte(sb, '"');
            }
            sb_cstr(sb, e->indirect ? ",\"direct\":false" : ",\"direct\":true");
            if (rep_by && rep_by->n) list_json(sb, "required_by", rep_by);
        }
        if (rep_at) {
            list_json(sb, "required_by", rep_by);
            sb_cstr(sb, ",\"required_version\":\"");
            sb_str(sb, rep_at);
            sb_byte(sb, '"');
        }
        if (now) list_json(sb, "effects", now);
        if (gained) list_json(sb, "gained", gained);
        if (lost) list_json(sb, "lost", lost);
        if (note) {
            sb_cstr(sb, ",\"note\":");
            json_str_into(sb, note);
        }
        sb_cstr(sb, "}\n");
    } else {
        if (cstr_eq(change, "added")) {
            sb_cstr(sb, "added ");
            sb_str(sb, e->name);
            sb_byte(sb, ' ');
            if (e->versioned) {
                sb_str(sb, pkg_ver_text(e->ver));
                sb_byte(sb, ' ');
            }
            sb_str(sb, e->raw);
            sb_byte(sb, ' ');
            sb_str(sb, pkg_hash_text(e->pin));
            if (rep_by && rep_by->n) {
                by_text(sb);
            } else {
                sb_cstr(sb, " from ");
                sb_str(sb, e->url);
            }
        } else if (cstr_eq(change, "updated")) {
            sb_cstr(sb, "updated ");
            sb_str(sb, e->name);
            if (e->versioned) { /* `updated NAME OLD -> NEW: HASH -> HASH`, or ` NEW:` when it had no version */
                sb_byte(sb, ' ');
                if (old_ver) {
                    sb_str(sb, old_ver);
                    sb_cstr(sb, " -> ");
                }
                sb_str(sb, pkg_ver_text(e->ver));
            }
            sb_cstr(sb, ": ");
            sb_str(sb, old_hash);
            sb_cstr(sb, " -> ");
            sb_str(sb, pkg_hash_text(e->pin));
            if (rep_by && rep_by->n) by_text(sb);
        } else {
            sb_cstr(sb, change);
            sb_byte(sb, ' ');
            sb_str(sb, e->name);
            if (e->versioned) {
                sb_byte(sb, ' ');
                sb_str(sb, pkg_ver_text(e->ver));
            }
            if (rep_by && rep_by->n && cstr_eq(change, "local")) by_text(sb);
        }
        sb_byte(sb, '\n');
        if (now) list_text(sb, "  effects: ", now);
        if (note) {
            sb_cstr(sb, "  effects: could not be read (");
            sb_str(sb, note);
            sb_cstr(sb, ")\n");
        }
        if (gained && gained->n) list_text(sb, "  gains: ", gained);
        if (lost && lost->n) list_text(sb, "  loses: ", lost);
    }
    out_bytes(1, sb->ptr, sb->len);
}

static Str *with_pid(Str *path, char *tag)
{
    SB *sb = sb_new();
    sb_str(sb, path);
    sb_cstr(sb, tag);
    int_to_dec(sb, sys_getpid());
    return sb_finish(sb);
}

/* 1 when the entry's directory is there and still hashes to its pin. */
static int pkg_intact(Pkg *e)
{
    char now[32];
    return pkg_hash(e->dir, now, 0) == 0 && mem_eq(now, e->pin, 32);
}

/* The entry's version (0: none), as the package the author asked for. */
static void pkg_set_version(Pkg *e, long *ver)
{
    e->versioned = ver != 0;
    e->indirect = 0;
    for (int k = 0; k < 3; k++) e->ver[k] = ver ? ver[k] : 0;
}

/* `vo package ENTRY NAME URL` (an archive): fetch, unpack, pin, record the URL. */
static int pkg_add_url(char *entry_arg, Str *name, Str *url)
{
    long at = 8, hash = 0;
    while (hash < url->len && url->data[hash] != '#') hash++;
    for (; at < hash && url->data[at] != '/'; at++)
        if (url->data[at] == '@') usage_error(str_from_cstr("A URL with a user name or password is not fetched."));
    if (hash < url->len) { /* #PATH: a folder inside the archive */
        char *bad = path_unsafe(url->data + hash + 1, url->len - hash - 1);
        if (bad || url->data[url->len - 1] == '/')
            usage_error(str_from_cstr("The `#PATH` of a package URL is a relative folder inside the archive, such as `#src/money`."));
    }
    Pkg *e = pkg_find(name);
    SB *sb = sb_new();
    if (opt_pkg_dir) {
        sb_cstr(sb, opt_pkg_dir);
        while (sb->len > 1 && sb->ptr[sb->len - 1] == '/') sb->len--;
    } else {
        sb_str(sb, pkg_dir);
        sb_cstr(sb, "/packages/");
        sb_str(sb, name);
    }
    Str *target = sb_finish(sb);
    int there = is_dir(target);
    Vec *before = 0;
    Str *note = 0;
    if (there) {
        Str *real = rt_resolve(target), *own = e ? rt_resolve(e->dir) : 0;
        if (!e || !real || !own || !str_eq(real, own))
            pkg_fail("E0706", msg("The directory `$1` exists and is not package `$2`'s: remove it, or choose another with `--dir`.",
                                  target, name, 0));
        if (!pkg_intact(e))
            pkg_fail("E0701", msg("Package `$1` was changed since it was pinned, and fetching would replace it: pin your "
                                  "changes with `vo package $2 $1 $3`, or remove the directory.",
                                  name, str_from_cstr(entry_arg), target));
        before = pkg_effects(e, &note);
    }
    Str *staging = with_pid(target, ".new-"), *old = with_pid(target, ".old-"), *why = 0;
    int r = pkg_stage(url, 0, staging, &why);
    long h = r ? 0 : pkg_hash(staging, pkg_hash_buf, 0);
    if (r || h < 0) {
        rm_tree(staging);
        if (r == -1) pkg_fail("E0706", msg("Cannot fetch package `$1` from $2: $3.", name, url, why));
        pkg_fail("E0702", msg("Package `$1`: $2.", name, r ? why : str_from_cstr("the archive's .vo files cannot be read back"), 0));
    }
    if (there && mem_eq(e->pin, pkg_hash_buf, 32)) { /* the same content: nothing to replace */
        rm_tree(staging);
        if (!e->url || !str_eq(e->url, url) || e->versioned) {
            e->url = url;
            pkg_set_version(e, 0);
            pkg_write();
        }
        pkg_report_fetched("unchanged", e, 0, 0, 0, 0);
        return 0;
    }
    if (there && sys_rename(target->data, old->data) < 0)
        pkg_fail("E0706", msg("Cannot replace the directory `$1`.", target, 0, 0));
    if (sys_rename(staging->data, target->data) < 0) {
        if (there) sys_rename(old->data, target->data);
        rm_tree(staging);
        pkg_fail("E0706", msg("Cannot create the directory `$1`.", target, 0, 0));
    }
    Str *abs = rt_resolve(target);
    Str *old_hash = e ? pkg_hash_text(e->pin) : 0;
    if (!e) {
        e = alloc(sizeof(Pkg));
        e->name = name;
        vec_push(pkg_entries, e);
    }
    e->dir = abs;
    e->raw = pkg_rel_dir(pkg_dir, abs);
    e->url = url;
    pkg_set_version(e, 0);
    e->state = 0;
    mem_copy(e->pin, pkg_hash_buf, 32);
    pkg_write();
    if (there) rm_tree(old);
    note = 0;
    Vec *now = pkg_effects(e, &note);
    pkg_report_fetched(old_hash ? "updated" : "added", e, old_hash, old_hash ? before : 0, now, note);
    return 0;
}

/* ---- spec 061: selection (research R4) ----
 * A program holds one version of each package: the highest that the program's manifest or the manifest
 * of any selected package requires. `sels` is that choice while a command works it out: every versioned
 * entry of the manifest, the package the command names, and what their manifests (the `vo.packages` in
 * each package's directory) require, to any depth. A package whose version is raised is fetched again
 * and read again; versions only rise, so this ends. Nothing on disk changes until sel_apply. */

typedef struct Sel {
    Str *name;
    Str *source; /* the repository, with its #FOLDER */
    long ver[3]; /* ver[0] < 0: the highest the source has (filled in when fetched) */
    char pin[32];
    Pkg *entry;   /* its manifest entry, or 0 for a new package */
    Str *target;  /* the directory it lives in, or will (absolute) */
    Str *staging; /* fetched content waiting to take the target's place, or 0 */
    Vec *by;      /* the packages that require it (sel_reach) */
    Vec *needs;   /* what its manifest requires (Req), at the version read */
    struct Sel *from; /* the package whose requirement brought it in, or 0: the program's manifest or the command */
    Str *raised;  /* who required the selected version, as text, when a requirement set it */
    long own[3];  /* has_own: the version the program itself asks for (a direct entry, or the command) */
    long has_own;
    long unknown; /* its requirements could not be read (not on disk as pinned) */
    long reach;   /* a direct package, or required by one that is reached */
    Vec *before;  /* its effects before, now, and why they could not be read */
    Vec *now;
    Str *note;
    Str *old_hash;
    Str *old_ver;
    long direct;  /* asked for by the program's author */
    long named;   /* the package this command names */
    long local;   /* a program entry without a version: it stays as it is */
    long visited; /* its content and requirements are read at this version */
    long added;   /* it had no manifest entry */
    long gone;     /* `--remove` names it */
    long restored; /* fetched again as pinned (`--repair` of a missing directory) */
    long changed; /* its directory was (or with --dry-run would be) replaced */
} Sel;

static Vec *sels;
static long sel_repair; /* `--repair`: a package not on disk as pinned is fetched, and every package reported */

/* Removes what was staged for s, and the folder made for it when nothing else is in it. */
static void sel_unstage(Sel *s)
{
    if (!s->staging) return;
    rm_tree(s->staging);
    long cut = s->staging->len;
    while (cut > 1 && s->staging->data[cut - 1] != '/') cut--;
    vo_syscall3(SYS_unlinkat, AT_FDCWD, (long)str_new(s->staging->data, cut - 1)->data, 0x200 /* AT_REMOVEDIR: only when empty */);
    s->staging = 0;
}

/* A failure during selection: what was staged goes, then the error (exit 65). */
static void sel_fail(char *code, Str *m)
{
    for (long i = 0; sels && i < sels->n; i++) sel_unstage(vec_get(sels, i));
    pkg_fail(code, m);
}

/* The selection of a name: the one there, or one made from its manifest entry, or 0. */
static Sel *sel_of(Str *name)
{
    for (long i = 0; i < sels->n; i++) {
        Sel *s = vec_get(sels, i);
        if (str_eq(s->name, name)) return s;
    }
    Pkg *e = pkg_find(name);
    if (!e) return 0;
    Sel *s = alloc(sizeof(Sel));
    s->name = name;
    s->entry = e;
    s->source = e->url;
    s->target = e->dir;
    s->local = !e->versioned;
    s->direct = !e->indirect;
    s->has_own = e->versioned && !e->indirect;
    for (int k = 0; k < 3; k++) s->own[k] = e->ver[k];
    s->by = vec_new();
    s->needs = vec_new();
    for (int k = 0; k < 3; k++) s->ver[k] = e->ver[k];
    mem_copy(s->pin, e->pin, 32);
    vec_push(sels, s);
    return s;
}

static Sel *sel_new(Str *name, Str *source, long *ver, Str *target)
{
    Sel *s = alloc(sizeof(Sel));
    s->name = name;
    s->source = source;
    s->target = target;
    s->by = vec_new();
    s->needs = vec_new();
    for (int k = 0; k < 3; k++) s->ver[k] = ver[k];
    if (sels->n >= 1000) sel_fail("E0706", str_from_cstr("More than 1,000 packages are required."));
    vec_push(sels, s);
    return s;
}

static int ver_cmp(long *a, long *b)
{
    for (int k = 0; k < 3; k++)
        if (a[k] != b[k]) return a[k] < b[k] ? -1 : 1;
    return 0;
}

/* Another version for s: what was fetched for the old one goes, and it is read again. */
static void sel_set(Sel *s, long *ver)
{
    for (int k = 0; k < 3; k++) s->ver[k] = ver[k];
    sel_unstage(s);
    s->visited = 0;
    s->needs = vec_new();
}

typedef struct Req { /* a line of a package's manifest; applied when the round that read it ends */
    Str *name;
    Str *source;
    long ver[3];
    char pin[32];
    Sel *from; /* the package that requires it */
} Req;

static Vec *sel_pending;

/* "`report` 2.1.0": a requirer at the version it has now. */
static Str *sel_at(Sel *s)
{
    return msg("`$1` $2", s->name, pkg_ver_text(s->ver), 0);
}

/* q is required: a new package, or a higher version of a selected one. What does not fit (another
 * source, another major version) is sel_check's to report. */
static void sel_require(Req *q)
{
    Sel *s = sel_of(q->name);
    if (!s) {
        SB *sb = sb_new();
        sb_str(sb, pkg_dir);
        sb_cstr(sb, "/packages/");
        sb_str(sb, q->name);
        s = sel_new(q->name, q->source, q->ver, sb_finish(sb));
        s->from = q->from;
        s->raised = sel_at(q->from);
    } else if (s->local) { /* the program's own directory for it stays; the report says what was asked */
        if (ver_cmp(q->ver, s->ver) > 0)
            for (int k = 0; k < 3; k++) s->ver[k] = q->ver[k];
    } else if (ver_cmp(q->ver, s->ver) > 0) {
        sel_set(s, q->ver);
        s->raised = sel_at(q->from);
    }
}

/* A source as compared between manifests: without one trailing `/` and one trailing `.git` of its path. */
static Str *src_norm(Str *u)
{
    long end = 0;
    while (end < u->len && u->data[end] != '#') end++;
    long cut = end;
    if (cut && u->data[cut - 1] == '/') cut--;
    if (cut >= 4 && mem_eq(u->data + cut - 4, ".git", 4)) cut -= 4;
    SB *sb = sb_new();
    sb_bytes(sb, u->data, cut);
    sb_bytes(sb, u->data + end, u->len - end);
    return sb_finish(sb);
}

/* Versions that may stand in for each other: the same major, and below 1.0.0 the same minor. */
static int compatible(long *a, long *b)
{
    return a[0] == b[0] && (a[0] || a[1] == b[1]);
}

static Str *sel_holder(Sel *s)
{
    return s->from ? msg("`$1`", s->from->name, 0, 0) : str_from_cstr(s->named ? "this command" : "this program");
}

/* E0707: every requirer of d with the version it asks, the program first, then by name. */
static void sel_conflict(Sel *d)
{
    Vec *who = vec_new(), *ver = vec_new(), *key = vec_new();
    int has = 0; /* someone listed asks for the selected version */
    if (d->has_own) {
        vec_push(key, str_from_cstr(""));
        vec_push(who, str_from_cstr(d->named ? "this command" : "this program"));
        vec_push(ver, pkg_ver_text(d->own));
        has |= !ver_cmp(d->own, d->ver);
    }
    for (long i = 0; i < sels->n; i++) {
        Sel *t = vec_get(sels, i);
        for (long k = 0; k < t->needs->n; k++) {
            Req *q = vec_get(t->needs, k);
            if (!str_eq(q->name, d->name)) continue;
            long at = who->n;
            vec_push(key, t->name);
            vec_push(who, msg("`$1`", t->name, 0, 0));
            vec_push(ver, pkg_ver_text(q->ver));
            has |= !ver_cmp(q->ver, d->ver);
            for (; at > 0 && str_cmp(vec_get(key, at - 1), vec_get(key, at)) > 0; at--) {
                Vec *vs[3] = {key, who, ver};
                for (int v = 0; v < 3; v++) {
                    void *x = vs[v]->data[at].p;
                    vs[v]->data[at].p = vs[v]->data[at - 1].p;
                    vs[v]->data[at - 1].p = x;
                }
            }
        }
    }
    if (!has) { /* asked by a version since replaced, or only pinned */
        vec_push(who, d->raised ? d->raised : str_from_cstr("this program's manifest"));
        vec_push(ver, pkg_ver_text(d->ver));
    }
    SB *sb = sb_new();
    for (long i = 0; i < who->n; i++) {
        if (i) sb_cstr(sb, ", ");
        sb_str(sb, vec_get(ver, i));
        sb_cstr(sb, " by ");
        sb_str(sb, vec_get(who, i));
    }
    sel_fail("E0707", msg("Package `$1` is required at incompatible versions: $2.", d->name, sb_finish(sb), 0));
}

/* What the selection must not hold (research R4 step 3), looked for when a round's requirements are in:
 * a name with two sources or a source with two names (E0708), versions of one package that cannot stand
 * in for each other (E0707), a version whose content is not what a requirer pinned (E0710). A program
 * entry without a version is the author's own choice and is not checked. */
static void sel_check(void)
{
    long n = sels->n;
    for (long i = 0; i < n; i++) {
        Sel *s = vec_get(sels, i);
        for (long k = 0; k < s->needs->n; k++) {
            Req *q = vec_get(s->needs, k);
            Sel *d = sel_of(q->name);
            if (d->local) continue;
            if (!str_eq(src_norm(q->source), src_norm(d->source)))
                sel_fail("E0708", msg("Package `$1` names two sources: $2, $3.", d->name, msg("$1 ($2)", d->source, sel_holder(d), 0),
                                      msg("$1 (`$2`)", q->source, s->name, 0)));
            if (!compatible(q->ver, d->ver)) sel_conflict(d);
            if (d->visited && !d->unknown && !ver_cmp(q->ver, d->ver) && !mem_eq(q->pin, d->pin, 32))
                sel_fail("E0710", msg("$1 has other content ($2) than $3: the version was changed after release.", sel_at(d), pkg_hash_text(d->pin),
                                      msg("`$1` pinned ($2)", s->name, pkg_hash_text(q->pin), 0)));
        }
    }
    /* ponytail: every pair, fine for the 1,000 packages a command may select; sort by source if that grows */
    for (long i = 0; i < n; i++) {
        Sel *a = vec_get(sels, i);
        for (long j = i + 1; j < n && !a->local; j++) {
            Sel *b = vec_get(sels, j);
            if (!b->local && str_eq(src_norm(a->source), src_norm(b->source)))
                sel_fail("E0708", msg("One source has two names: $1 and $2: $3.", msg("`$1` ($2)", a->name, sel_holder(a), 0),
                                      msg("`$1` ($2)", b->name, sel_holder(b), 0), a->source));
        }
    }
}

/* The requirements of s: the versioned lines of the `vo.packages` in dir. */
static void sel_requirements(Sel *s, Str *dir)
{
    char *text;
    long n = read_whole_file(path_join(dir, str_from_cstr("vo.packages"))->data, &text), row = 0;
    for (char *line = text, *end = text + n; n > 0 && line < end; row++) {
        char *le = line;
        while (le < end && *le != '\n') le++;
        int problem;
        Pkg *r = pkg_line(line, le, &problem);
        if (problem)
            sel_fail("E0703", msg("Package `$1` $2: line $3 of its `vo.packages` is malformed.", s->name, pkg_ver_text(s->ver),
                                  int_to_str(row + 1)));
        if (r && !r->versioned) /* a directory or an archive of the author's own: nobody else can fetch it as a version */
            sel_fail("E0709", msg("Package `$1` $2 cannot be required: `$3` has no source in its manifest (a requirement is "
                                  "written with `https://HOST/PATH@VERSION`).",
                                  s->name, pkg_ver_text(s->ver), r->name));
        if (r && r->versioned && !str_eq(r->name, s->name)) {
            Req *q = alloc(sizeof(Req));
            q->name = r->name;
            q->source = r->url;
            q->from = s;
            for (int k = 0; k < 3; k++) q->ver[k] = r->ver[k];
            mem_copy(q->pin, r->pin, 32);
            vec_push(sel_pending, q);
            vec_push(s->needs, q);
        }
        line = le + 1;
    }
}

/* Reads s at its version: from its directory when the manifest has it so, else fetched into staging. */
static void sel_visit(Sel *s)
{
    Pkg *e = s->entry;
    s->visited = 1;
    if (s->local) return;
    int same = e && e->versioned && str_eq(e->url, s->source) && !ver_cmp(e->ver, s->ver);
    if (same && pkg_intact(e)) {
        mem_copy(s->pin, e->pin, 32); /* (a version the command asked for and a requirement overruled left its own) */
        sel_requirements(s, e->dir);
        return;
    }
    if (same && !s->named && !sel_repair) { /* not on disk as pinned: `--restore`'s to bring back, `vo check`'s to report */
        s->unknown = 1;
        return;
    }
    int foreign = 0; /* a directory is there that no entry accounts for: replaced only by the same content */
    if (is_dir(s->target)) {
        Str *real = rt_resolve(s->target), *own = e ? rt_resolve(e->dir) : 0;
        foreign = !e || !real || !own || !str_eq(real, own);
        if (!foreign && !pkg_intact(e))
            sel_fail("E0701", msg("Package `$1` was changed since it was pinned, and fetching would replace it: pin your "
                                  "changes with `vo package ENTRY $1 $2`, or remove the directory.",
                                  s->name, e->raw, 0));
    }
    Str *staging = with_pid(s->target, ".new-"), *why = 0;
    s->staging = staging;
    int r = pkg_stage(s->source, s->ver, staging, &why);
    long h = r ? 0 : pkg_hash(staging, s->pin, 0);
    char there[32];
    if (!r && h >= 0 && foreign && (pkg_hash(s->target, there, 0) != 0 || !mem_eq(there, s->pin, 32)))
        sel_fail("E0706", msg("The directory `$1` exists and is not package `$2`'s: remove it, or choose another with `--dir`.",
                              s->target, s->name, 0));
    if (s->named && s->own[0] < 0)
        for (int k = 0; k < 3; k++) s->own[k] = s->ver[k]; /* the highest: now known */
    if (r == -3) sel_fail("E0711", msg("$1.", why, 0, 0));
    if (r == -1) sel_fail("E0706", msg("Cannot fetch package `$1` from $2: $3.", s->name, s->source, why));
    if (r || h < 0) sel_fail("E0702", msg("Package `$1`: $2.", s->name, r ? why : str_from_cstr("its .vo files cannot be read back"), 0));
    if (e && e->versioned && str_eq(e->url, s->source) && !ver_cmp(e->ver, s->ver)) { /* the version the manifest has */
        if (!mem_eq(s->pin, e->pin, 32))
            sel_fail("E0701", msg("Package `$1`: version $2 now hashes to $3, not to the pinned hash: the version was "
                                  "changed after it was pinned. Nothing was written.",
                                  s->name, pkg_ver_text(s->ver), pkg_hash_text(s->pin)));
        if (pkg_intact(e)) { /* asked for again (the highest is the one there): nothing to replace */
            sel_unstage(s);
            sel_requirements(s, e->dir);
            return;
        }
    }
    sel_requirements(s, staging);
}

/* Selection from the roots in sels, in rounds: every package not yet read at its version is read, then
 * what they require is applied (a new package, or a higher version of one, read in the next round), until
 * a round reads nothing. A round's result does not depend on the order within it, so the selection is a
 * function of the manifest and the command alone. A version read and then raised still counted: versions
 * never go down within a command, as they never go down between commands. */
static void sel_run(void)
{
    for (;;) {
        long n = sels->n, read = 0;
        sel_pending = vec_new();
        for (long i = 0; i < n; i++) {
            Sel *s = vec_get(sels, i);
            if (s->visited) continue;
            sel_visit(s);
            read++;
        }
        if (!read) return;
        for (long i = 0; i < sel_pending->n; i++) sel_require(vec_get(sel_pending, i));
        sel_check();
    }
}

/* Marks what the program's own packages reach through requirements, and fills each package's `by`.
 * An indirect package that is not reached is one only an abandoned version (or a removed package)
 * required: sel_apply drops it. When a reached package could not be read, everything counts as reached. */
static void sel_reach(void)
{
    int unknown = 0;
    for (long i = 0; i < sels->n; i++) {
        Sel *s = vec_get(sels, i);
        s->reach = !s->gone && (s->direct || s->local);
    }
    for (int more = 1; more;) {
        more = 0;
        for (long i = 0; i < sels->n; i++) {
            Sel *s = vec_get(sels, i);
            if (!s->reach) continue;
            unknown |= s->unknown != 0;
            for (long k = 0; k < s->needs->n; k++) {
                Sel *d = sel_of(((Req *)vec_get(s->needs, k))->name);
                long j = 0;
                while (j < d->by->n && !str_eq(vec_get(d->by, j), s->name)) j++;
                if (j == d->by->n) vec_push(d->by, s->name);
                if (!d->reach) d->reach = more = 1;
            }
        }
    }
    for (long i = 0; unknown && i < sels->n; i++) ((Sel *)vec_get(sels, i))->reach = !((Sel *)vec_get(sels, i))->gone;
}

static Vec *set_union(Vec *a, Vec *b)
{
    Vec *out = vec_new();
    for (long i = 0; a && i < a->n; i++) vec_push(out, vec_get(a, i));
    Vec *rest = set_minus(b, a);
    for (long i = 0; i < rest->n; i++) vec_push(out, vec_get(rest, i));
    return out;
}

/* Makes the selection the program's: staged packages take their directories, the manifest is written
 * last, each package is reported (contracts/package-command.md). With --dry-run only the report. */
static int sel_apply(void)
{
    long n = sels->n, changed = 0, meta = 0;
    sel_reach();
    for (long i = 0; i < n; i++) { /* what nothing requires any more leaves the program */
        Sel *s = vec_get(sels, i);
        if (s->reach) continue;
        sel_unstage(s);
        if (!s->entry) continue;
        long k = 0;
        while (pkg_entries->data[k].p != s->entry) k++;
        pkg_entries->n--;
        pkg_entries->data[k] = pkg_entries->data[pkg_entries->n];
        meta = 1;
    }
    for (long i = 1; i < n; i++) /* name order */
        for (long j = i; j > 0 && str_cmp(((Sel *)vec_get(sels, j - 1))->name, ((Sel *)vec_get(sels, j))->name) > 0; j--) {
            void *t = sels->data[j].p;
            sels->data[j].p = sels->data[j - 1].p;
            sels->data[j - 1].p = t;
        }
    for (long i = 0; i < n; i++) { /* what each replaced package declared */
        Sel *s = vec_get(sels, i);
        if (s->staging && s->entry && pkg_intact(s->entry)) s->before = pkg_effects(s->entry, &s->note);
    }
    for (long i = 0; i < n; i++) { /* the entries as they will be, their directories still the staged ones */
        Sel *s = vec_get(sels, i);
        Pkg *e = s->entry;
        if (!s->staging) {
            if (e && !s->local && e->indirect == s->direct) { /* asked for by name now */
                e->indirect = !s->direct;
                meta = 1;
            }
            continue;
        }
        changed++;
        if (!e) {
            e = alloc(sizeof(Pkg));
            e->name = s->name;
            long skip = pkg_dir->len + 1;
            e->raw = s->target->len > skip && mem_eq(s->target->data, pkg_dir->data, pkg_dir->len) ? str_new(s->target->data + skip, s->target->len - skip) : s->target;
            vec_push(pkg_entries, e);
            s->entry = e;
            s->added = 1;
        } else {
            s->old_hash = pkg_hash_text(e->pin);
            s->old_ver = e->versioned ? pkg_ver_text(e->ver) : 0;
            s->restored = e->versioned && !ver_cmp(e->ver, s->ver) && mem_eq(e->pin, s->pin, 32); /* was missing */
        }
        e->dir = s->staging;
        e->url = s->source;
        e->versioned = 1;
        e->indirect = !s->direct;
        e->state = 0;
        for (int k = 0; k < 3; k++) e->ver[k] = s->ver[k];
        mem_copy(e->pin, s->pin, 32);
    }
    for (long i = 0; i < n; i++) {
        Sel *s = vec_get(sels, i);
        s->note = 0;
        if (s->staging) s->now = pkg_effects(s->entry, &s->note);
    }
    if (!opt_dry_run) {
        long done = 0;
        Str *failed = 0;
        for (; done < n && !failed; done++) { /* the old directory aside, the new one in */
            Sel *s = vec_get(sels, done);
            if (!s->staging) continue;
            Str *old = with_pid(s->target, ".old-");
            int there = is_dir(s->target);
            if (there && sys_rename(s->target->data, old->data) < 0) failed = s->target;
            else if (sys_rename(s->staging->data, s->target->data) < 0) {
                if (there) sys_rename(old->data, s->target->data);
                failed = s->target;
            }
        }
        if (failed) { /* back to how it was */
            for (long i = done - 2; i >= 0; i--) {
                Sel *s = vec_get(sels, i);
                if (!s->staging) continue;
                Str *old = with_pid(s->target, ".old-");
                sys_rename(s->target->data, s->staging->data);
                if (is_dir(old)) sys_rename(old->data, s->target->data);
            }
            sel_fail("E0706", msg("Cannot replace the directory `$1`.", failed, 0, 0));
        }
        for (long i = 0; i < n; i++) {
            Sel *s = vec_get(sels, i);
            if (!s->staging) continue;
            Str *abs = rt_resolve(s->target);
            s->entry->dir = abs ? abs : s->target;
            if (abs) s->entry->raw = pkg_rel_dir(pkg_dir, abs);
        }
        if (changed || meta) pkg_write();
        for (long i = 0; i < n; i++) { /* a dropped package's directory goes when it is what was fetched */
            Sel *s = vec_get(sels, i);
            if (!s->reach && s->entry && pkg_intact(s->entry)) rm_tree(s->entry->dir);
        }
    }
    for (long i = 0; i < n; i++) { /* the old directories go, or with --dry-run what was staged */
        Sel *s = vec_get(sels, i);
        s->changed = s->staging != 0; /* (a dry run clears staging) */
        if (!s->staging) continue;
        if (opt_dry_run) sel_unstage(s);
        else rm_tree(with_pid(s->target, ".old-"));
    }
    if (opt_dry_run && !opt_json) out_cstr(1, "dry run: nothing written\n");
    Vec *gained = vec_new(), *lost = vec_new();
    for (long i = 0; i < n; i++) {
        Sel *s = vec_get(sels, i);
        rep_by = s->direct && (s->named || !s->changed) ? 0 : s->by; /* the program's own, raised: by whom */
        old_ver = s->restored ? 0 : s->old_ver;
        if (!s->reach) {
            rep_by = 0;
            if (s->entry) pkg_report_fetched("removed", s->entry, 0, 0, 0, 0);
        } else if (s->restored) {
            pkg_report_fetched("restored", s->entry, 0, 0, 0, 0);
        } else if (s->changed) {
            Vec *before = s->added ? 0 : s->before;
            pkg_report_fetched(s->added ? "added" : "updated", s->entry, s->old_hash, before, s->now, s->note);
            gained = set_union(gained, s->added ? s->now : set_minus(s->now, before));
            lost = set_union(lost, set_minus(before, s->now));
        } else if (s->local && s->by->n) {
            rep_by = s->by;
            rep_at = pkg_ver_text(s->ver);
            pkg_report_fetched("local", s->entry, 0, 0, 0, 0);
            rep_at = 0;
        } else if (s->named || (sel_repair && !s->local)) {
            pkg_report_fetched("unchanged", s->entry, 0, 0, 0, 0);
        }
    }
    rep_by = 0;
    old_ver = 0;
    if (changed > 1) {
        SB *sb = sb_new();
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"package_total\"");
            list_json(sb, "gained", gained);
            list_json(sb, "lost", lost);
            sb_cstr(sb, "}\n");
        } else {
            if (gained->n) list_text(sb, "total gains: ", gained);
            if (lost->n) list_text(sb, "total loses: ", lost);
        }
        out_bytes(1, sb->ptr, sb->len);
    }
    return 0;
}

/* The roots every command starts from: the versioned entries of the manifest. */
static void sel_start(void)
{
    sels = vec_new();
    for (long i = 0; i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        if (e->versioned) sel_of(e->name);
    }
}

/* `vo package ENTRY NAME SOURCE[@VERSION]`: the package, as the author's own, and what it requires. */
static int pkg_add_source(Str *name, Str *source, long *ver)
{
    sel_start();
    Sel *s = sel_of(name);
    if (!s) {
        SB *sb = sb_new();
        if (opt_pkg_dir) {
            if (opt_pkg_dir[0] != '/') {
                sb_str(sb, rt_start_cwd());
                sb_byte(sb, '/');
            }
            sb_cstr(sb, opt_pkg_dir);
            while (sb->len > 1 && sb->ptr[sb->len - 1] == '/') sb->len--;
        } else {
            sb_str(sb, pkg_dir);
            sb_cstr(sb, "/packages/");
            sb_str(sb, name);
        }
        s = sel_new(name, source, ver, sb_finish(sb));
    } else {
        s->source = source;
        s->local = 0;
        sel_set(s, ver);
    }
    s->direct = 1;
    s->named = 1;
    s->has_own = 1;
    for (int k = 0; k < 3; k++) s->own[k] = ver[k];
    sel_run();
    return sel_apply();
}

/* `vo package --repair ENTRY`: the selection the manifest's own entries give, made the program's: what is
 * required and missing is added, what is too low raised, what nothing requires dropped, what is not on
 * disk fetched as pinned. */
static int pkg_repair(void)
{
    sel_repair = 1;
    sel_start();
    sel_run();
    return sel_apply();
}

/* `vo package --remove ENTRY NAME` for a versioned package: refused while a listed package requires it
 * (E0709); else it goes, with the indirect packages only it required. Read from disk: nothing is fetched. */
static int pkg_remove_versioned(Str *name)
{
    sel_start();
    sel_run();
    SB *sb = sb_new();
    for (long i = 0; i < sels->n; i++) {
        Sel *t = vec_get(sels, i);
        for (long k = 0; k < t->needs->n; k++) {
            if (!str_eq(((Req *)vec_get(t->needs, k))->name, name)) continue;
            sb_cstr(sb, sb->len ? ", `" : "`");
            sb_str(sb, t->name);
            sb_byte(sb, '`');
        }
    }
    if (sb->len) sel_fail("E0709", msg("Package `$1` is required by $2: nothing was removed.", name, sb_finish(sb), 0));
    sel_of(name)->gone = 1;
    return sel_apply();
}

/* What `source` has above `cur`: bit 1 and `compat`, the highest version that can stand in for it; bit 2
 * and `major`, the highest version that cannot (a new major; below 1.0.0 a new minor). */
static int pkg_newer(Str *source, long *cur, long *compat, long *major)
{
    long h = 0;
    while (h < source->len && source->data[h] != '#') h++;
    Str *repo = str_new(source->data, h), *final = 0, *why = 0;
    Vec *tags = git_versions(repo, &final, &why); /* ascending */
    if (!tags) pkg_fail("E0706", msg("Cannot read the versions of $1: $2.", repo, why, 0));
    int r = 0;
    for (long i = 0; i < tags->n; i++) {
        GitTag *t = vec_get(tags, i);
        if (ver_cmp(t->v, cur) <= 0) continue;
        int fits = compatible(t->v, cur);
        r |= fits ? 1 : 2;
        for (int k = 0; k < 3; k++) (fits ? compat : major)[k] = t->v[k];
    }
    return r;
}

/* `vo package --update ENTRY [NAME]`: NAME, or every package the program asks for itself, at the highest
 * version compatible with the one it has; then selection as for an add. */
static int pkg_update(Str *name)
{
    sel_start();
    for (long i = 0; i < sels->n; i++) {
        Sel *s = vec_get(sels, i);
        long compat[3], major[3];
        if (name ? !str_eq(s->name, name) : !s->direct) continue;
        if (pkg_newer(s->source, s->ver, compat, major) & 1) sel_set(s, compat);
        s->named = s->has_own = 1;
        for (int k = 0; k < 3; k++) s->own[k] = s->ver[k];
    }
    sel_run();
    return sel_apply();
}

/* `vo package --outdated ENTRY`: each versioned package that has something newer. Nothing is written.
 * ponytail: one request per package; remember the answer per repository when monorepos make that slow. */
static int pkg_outdated(void)
{
    int any = 0;
    pkg_sort();
    for (long i = 0; i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        long compat[3], major[3];
        int r = e->versioned ? pkg_newer(e->url, e->ver, compat, major) : 0;
        if (!r) continue;
        any = 1;
        SB *sb = sb_new();
        Str *c = r & 1 ? pkg_ver_text(compat) : 0, *m = r & 2 ? pkg_ver_text(major) : 0;
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"package\",\"name\":");
            json_str_into(sb, e->name);
            sb_cstr(sb, ",\"change\":\"outdated\",\"version\":\"");
            sb_str(sb, pkg_ver_text(e->ver));
            sb_cstr(sb, "\",\"compatible\":");
            if (c) json_str_into(sb, c);
            else sb_cstr(sb, "null");
            sb_cstr(sb, ",\"newer_major\":");
            if (m) json_str_into(sb, m);
            else sb_cstr(sb, "null");
            sb_cstr(sb, "}\n");
        } else {
            sb_str(sb, msg("$1 $2: ", e->name, pkg_ver_text(e->ver), 0));
            if (c) sb_str(sb, msg(m ? "$1 available; " : "$1 available", c, 0, 0));
            if (m) sb_str(sb, msg("$1 is a new major (add it with @$1)", m, 0, 0));
            sb_byte(sb, '\n');
        }
        out_bytes(1, sb->ptr, sb->len);
    }
    if (!any && !opt_json) out_cstr(1, "all packages are at their newest version\n");
    return 0;
}

/* `vo package --restore ENTRY`: every entry with a URL whose directory is missing is fetched again
 * and accepted only at its pinned hash. The manifest is never written. */
static int pkg_restore(void)
{
    int failed = 0;
    pkg_sort();
    for (long i = 0; i < pkg_entries->n; i++) {
        Pkg *e = vec_get(pkg_entries, i);
        char now[32];
        long h = pkg_hash(e->dir, now, 0);
        char *code = 0;
        Str *m = 0;
        if (h == 0 && mem_eq(now, e->pin, 32)) {
            pkg_report_fetched("present", e, 0, 0, 0, 0);
            continue;
        }
        if (h == 0 || is_dir(e->dir)) { /* there, and not what was pinned: not ours to replace */
            code = h == 0 ? "E0701" : "E0702";
            m = h == 0 ? msg("Package `$1` changed: the manifest pins $2, its files hash to $3.", e->name, pkg_hash_text(e->pin),
                             pkg_hash_text(now))
                       : msg(h == -2 ? M_UNREADABLE : M_MISSING, e->name, e->raw, 0);
        } else if (!e->url) {
            code = "E0702";
            m = msg("Package `$1`: the directory `$2` is missing, and the manifest has no URL to fetch it from.", e->name, e->raw, 0);
        } else {
            Str *staging = with_pid(e->dir, ".new-"), *why = 0;
            long want[3] = {e->ver[0], e->ver[1], e->ver[2]};
            int r = pkg_stage(e->url, e->versioned ? want : 0, staging, &why);
            long sh = r ? -1 : pkg_hash(staging, now, 0);
            if (r == -3) {
                code = "E0711";
                m = msg("Package `$1`: $2.", e->name, why, 0);
            } else if (r == -1) {
                code = "E0706";
                m = msg("Cannot fetch package `$1` from $2: $3.", e->name, e->url, why);
            } else if (r || sh < 0) {
                code = "E0702";
                m = msg("Package `$1`: $2.", e->name, r ? why : str_from_cstr("the archive's .vo files cannot be read back"), 0);
            } else if (!mem_eq(now, e->pin, 32)) {
                code = "E0701";
                m = e->versioned ? msg("Package `$1`: version $2 now hashes to $3, not to the pinned hash: the version was "
                                       "changed after it was pinned. Nothing was written.",
                                       e->name, pkg_ver_text(e->ver), pkg_hash_text(now))
                                 : msg("Package `$1`: what $2 serves now hashes to $3, not to the pinned hash: the content at the URL "
                                       "changed. Nothing was written.",
                                       e->name, e->url, pkg_hash_text(now));
            } else if (sys_rename(staging->data, e->dir->data) < 0) {
                code = "E0706";
                m = msg("Cannot create the directory `$1`.", e->raw, 0, 0);
            }
            if (code) rm_tree(staging);
            else pkg_report_fetched("restored", e, 0, 0, 0, 0);
        }
        if (code) {
            toolchain_error((int)opt_json, (int)report_fd, code, m, 0);
            failed = 1;
        }
    }
    return failed ? EXIT_CHECK : 0;
}

/* `vo package [--json] ENTRY NAME DIRECTORY` and `vo package --remove ENTRY NAME`. */
static int ends_with(char *p, long n, char *lit)
{
    long k = cstr_len(lit);
    return n >= k && mem_eq(p + n - k, lit, k);
}

/* `vo package --unpack-pack PACK ID DIR [FOLDER]` (spec 061, a test entry: tests/package_git.py): the
 * package files of the commit or tag ID in the pack file, written under DIR. */
static int pkg_unpack_pack(void)
{
    char **args = (char **)pos_args;
    char id[32], *why = 0, *pack = 0, hex[65];
    Str *w = 0;
    long n = pos_n >= 3 ? read_whole_file(args[0], &pack) : -1;
    for (int i = 0; i < 64; i++) hex[i] = pos_n >= 3 && i < 40 && cstr_len(args[1]) == 40 ? args[1][i] : '0';
    if (n < 0 || cstr_len(args[1]) != 40 || !pkg_hex_decode(hex, id)) usage_error(str_from_cstr("`--unpack-pack` takes PACK ID DIR [FOLDER]."));
    Vec *files = git_unpack(pack, n, id, pos_n > 3 ? str_from_cstr(args[3]) : 0, &why);
    if (!files) pkg_fail("E0706", msg("Cannot read the pack: $1.", str_from_cstr(why), 0, 0));
    if (!pkg_has_vo(files)) pkg_fail("E0702", str_from_cstr("The pack has no .vo files there."));
    if (pkg_write_files(str_from_cstr(args[2]), files, &w)) pkg_fail("E0706", w);
    git_release();
    return 0;
}

int cmd_package(void)
{
    char **args = (char **)pos_args;
    if (opt_unpack_pack) return pkg_unpack_pack();
    long whole = opt_restore + opt_repair + opt_outdated + (opt_update && pos_n == 1); /* the forms that take ENTRY alone */
    int by_url = pos_n == 3 && doc_starts(args[2], "https://");
    if (opt_remove + opt_restore + opt_repair + opt_outdated + opt_update > 1 || pos_n != (whole ? 1 : 3 - (opt_remove || opt_update))) /* ENTRY [NAME [DIRECTORY | URL]] */
        usage_error(str_from_cstr("`package` takes ENTRY NAME DIRECTORY, ENTRY NAME URL, --update ENTRY [NAME], --outdated ENTRY, --repair ENTRY, "
                                  "--restore ENTRY, or --remove ENTRY NAME."));
    if (pos_n == 3 && doc_starts(args[2], "http://"))
        usage_error(str_from_cstr("A package is fetched from an https:// URL only."));
    pkg_command = 1;
    if (opt_dry_run && !by_url && !opt_repair && !opt_update && !opt_remove) usage_error(str_from_cstr("`--dry-run` goes with a versioned package, `--update`, `--repair` or `--remove`."));
    if (opt_pkg_dir && !by_url) usage_error(str_from_cstr("`--dir` goes with a URL: `vo package --dir DIRECTORY ENTRY NAME URL`."));
    if (opt_ca_files && !by_url && !whole && !opt_update)
        usage_error(str_from_cstr("`--ca-file` goes with a URL, `--update`, `--outdated`, `--repair` or `--restore`: the other forms fetch nothing."));
    if (!whole && !pkg_name_ok(args[1], cstr_len(args[1])))
        usage_error_arg("A package name is an identifier other than std: `$1` is not.", args[1]);
    Str *name = whole ? 0 : str_from_cstr(args[1]);
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
    if (opt_restore) return pkg_restore();
    if (opt_repair) return pkg_repair();
    if (opt_outdated) return pkg_outdated();
    if (opt_update) {
        Pkg *u = name ? pkg_find(name) : 0;
        if (name && !u) usage_error_arg("No package `$1` is listed in the manifest.", args[1]);
        if (name && !u->versioned)
            usage_error_arg("Package `$1` has no version to update from: pin it again with `vo package ENTRY $1 DIRECTORY`, or add it "
                            "from its repository with `vo package ENTRY $1 SOURCE`.", args[1]);
        return pkg_update(name);
    }
    if (by_url) { /* spec 061 (research R3): a repository unless the path ends as an archive does */
        Str *url = str_from_cstr(args[2]);
        long ver[3] = {-1, 0, 0}, hash = 0, at = url->len;
        while (at > 8 && url->data[at - 1] != '@' && url->data[at - 1] != '/') at--;
        int has_ver = at > 9 && url->data[at - 1] == '@' && cstr_index(url->data + 8, '/') >= 0 && cstr_index(url->data + 8, '/') < at - 9;
        if (has_ver) {
            if (url->data[at] == 'v' || !git_version_parse(url->data + at, url->len - at, ver))
                usage_error(str_from_cstr("A version is written `SOURCE@MAJOR.MINOR.PATCH`, such as `@1.2.0`."));
            url = str_new(url->data, at - 1);
        }
        while (hash < url->len && url->data[hash] != '#' && url->data[hash] != '?') hash++; /* the path's end */
        int archive = !has_ver && (ends_with(url->data, hash, ".tar") || ends_with(url->data, hash, ".tar.gz") || ends_with(url->data, hash, ".tgz"));
        if (opt_dry_run && archive) usage_error(str_from_cstr("`--dry-run` goes with a versioned package, `--update`, `--repair` or `--remove`."));
        if (!archive) { /* the checks the archive form makes on its URL */
            for (long i = 8; i < url->len && url->data[i] != '/'; i++)
                if (url->data[i] == '@') usage_error(str_from_cstr("A URL with a user name or password is not fetched."));
            long h = 0;
            while (h < url->len && url->data[h] != '#') h++;
            if (h < url->len && (path_unsafe(url->data + h + 1, url->len - h - 1) || url->data[url->len - 1] == '/'))
                usage_error(str_from_cstr("The `#FOLDER` of a package source is a relative folder inside the repository, such as `#libs/money`."));
            return pkg_add_source(name, url, ver);
        }
        return pkg_add_url(args[0], name, url);
    }
    Pkg *e = pkg_find(name); /* the entry, or 0 */
    if (opt_remove) {
        if (!e) usage_error_arg("No package `$1` is listed in the manifest.", args[1]);
        if (e->versioned) return pkg_remove_versioned(name);
        if (opt_dry_run) usage_error(str_from_cstr("`--remove --dry-run` goes with a versioned package: removing a directory's entry changes one line."));
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
        if (e->url) { /* spec 041: pinned by hand now, so no longer "from" anywhere */
            e->url = 0;
            pkg_set_version(e, 0);
            pkg_write();
        }
        pkg_report("unchanged", e, 0);
        return 0;
    }
    Str *old = pkg_hash_text(e->pin); /* updated */
    e->url = 0;
    pkg_set_version(e, 0);
    e->raw = raw;
    e->dir = abs;
    mem_copy(e->pin, pkg_hash_buf, 32);
    pkg_write();
    pkg_report("updated", e, old);
    return 0;
}
