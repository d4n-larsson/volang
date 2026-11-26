/* Spec 038: `vo lsp`, the toolchain as a language server (contracts/lsp.md).
 *
 * The server process reads messages and keeps the open documents; it computes nothing itself. Every
 * answer comes from a forked copy (as in src/mcp.c): the copy sees the open documents in its memory,
 * serves them to the loader in place of the files on disk (read_overlay), runs the ordinary command
 * with its output kept in memory (out_capture) and nothing written (write_capture), and when the
 * command ends (lsp_finish) turns its schema-1 records into the protocol's answer, converting
 * positions with the file texts at hand. So an answer is the command's answer, and a crash, a hang or
 * memory exhaustion ends with the copy. The server forwards a copy's output only when it ended well.
 *
 * Memory: document texts, messages and collected output are mapped and unmapped; nothing here is a
 * large static array (the collector scans static data in every vo process). */
#include "vo.h"

#define MAX_DOCS 512
#define PATH_CAP 4096
#define MSG_LIMIT (64L << 20)
#define PAUSE_MS 150 /* after the last change, before checking */
#define LIMIT_MS 5000 /* one copy */
#define SIGPIPE 13
#define SIGCHLD 17
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define F_SETFL 4
#define POLLIN 1
#define POLLERR 8
#define POLLHUP 16
#define WNOHANG 1
#define E_INTR 4
#define E_AGAIN 11
#define SITE "https://d4n-larsson.github.io/volang/errors/"

typedef struct PollFd {
    int fd;
    short events;
    short revents;
} PollFd;

typedef struct Buf { /* mapped, grows by doubling */
    char *p;
    long len, cap;
} Buf;

typedef struct Doc { /* an open document: one mapping holding its strings */
    char *map;
    long map_len;
    char *uri;   /* as the editor sent it */
    char *path;  /* its file */
    char *entry; /* PATH_CAP bytes: the entry file of its program as last found, or "" */
    char *text;  /* followed by a NUL */
    long len, version, dirty;
} Doc;

enum { K_DIAG = 1, K_PROBE, K_ACTION, K_HOVER, K_DEFINITION, K_PREPARE, K_REFS, K_RENAME, K_FORMAT, K_SYMBOLS };
enum { R_OK, R_FAILED, R_ABORTED, R_CANCELLED };

static Doc docs[MAX_DOCS];
static long ndocs;
static Buf inb;                 /* unread input */
static long in_pos, in_scan, in_eof;
static Buf pub;                 /* "ENTRY\tFILE\n": files with published diagnostics, per entry */
static Buf notes;               /* what the last copy told the server */
static char root[PATH_CAP];     /* the workspace folder, or "" */
static char rq_path[PATH_CAP];  /* the request's file */
static char rq_uri[PATH_CAP];
static char id[256];            /* the request's id as its JSON text */
static long id_len;
static int initialized, shutdown_seen, utf8_positions, warned, dirty;
static long change_ms;
static int sig_fd = -1;
static unsigned long old_mask;
/* the request a copy serves */
static int rq_kind;
static Doc *rq_doc;    /* its document when open */
static char *rq_entry; /* the program's entry file */
static char *rq_params;

/* ---- memory ---- */

static void leave(int code) NORETURN;
static void leave(int code)
{
    vo_syscall1(SYS_exit_group, code);
    for (;;) {}
}

static char *pages_new(long n)
{
    long r = vo_syscall6(SYS_mmap, 0, n, 3 /* read, write */, 0x22 /* private, anonymous */, -1, 0);
    if ((unsigned long)r >= (unsigned long)-4095L) leave(125);
    return (char *)r;
}

static void pages_free(char *p, long n)
{
    if (p) vo_syscall2(SYS_munmap, (long)p, n);
}

static void buf_need(Buf *b, long extra)
{
    if (b->len + extra + 1 <= b->cap) return;
    long cap = b->cap ? b->cap : 65536;
    while (cap < b->len + extra + 1) cap *= 2;
    char *n = pages_new(cap);
    mem_copy(n, b->p, b->len);
    pages_free(b->p, b->cap);
    b->p = n;
    b->cap = cap;
}

static void buf_add(Buf *b, char *p, long n)
{
    buf_need(b, n);
    mem_copy(b->p + b->len, p, n);
    b->len += n;
}

static void buf_str(Buf *b, char *s)
{
    buf_add(b, s, cstr_len(s));
}

static void buf_drop(Buf *b)
{
    pages_free(b->p, b->cap);
    b->p = 0;
    b->len = b->cap = 0;
}

static long fmt_int(char *buf, long v)
{
    char t[24];
    long n = 0, k = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (v < 0) buf[k++] = '-';
    while (n) buf[k++] = t[--n];
    return k;
}

static void buf_int(Buf *b, long v)
{
    char t[24];
    buf_add(b, t, fmt_int(t, v));
}

/* ---- sending (the server) ---- */

static void send_all(char *p, long n)
{
    for (long done = 0; done < n;) {
        long r = sys_write(1, p + done, n - done);
        if (r == -E_INTR) continue;
        if (r == -E_AGAIN) {
            sys_wait_fd(1, 4 /* POLLOUT */, -1);
            continue;
        }
        if (r <= 0) leave(1); /* the editor is gone */
        done += r;
    }
}

static void send_body(Buf *body)
{
    char head[64] = "Content-Length: ";
    long k = cstr_len(head);
    k += fmt_int(head + k, body->len);
    mem_copy(head + k, "\r\n\r\n", 4);
    send_all(head, k + 4);
    send_all(body->p, body->len);
}

static void reply(char *result)
{
    Buf b = {0};
    buf_str(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
    buf_add(&b, id, id_len);
    buf_str(&b, ",\"result\":");
    buf_str(&b, result);
    buf_str(&b, "}");
    send_body(&b);
    buf_drop(&b);
}

/* message is plain text without quotes or backslashes. */
static void reply_error(long code, char *message)
{
    Buf b = {0};
    buf_str(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id_len) buf_add(&b, id, id_len);
    else buf_str(&b, "null");
    buf_str(&b, ",\"error\":{\"code\":");
    buf_int(&b, code);
    buf_str(&b, ",\"message\":\"");
    buf_str(&b, message);
    buf_str(&b, "\"}}");
    send_body(&b);
    buf_drop(&b);
}

/* `file:///a%20b/c.vo` -> `/a b/c.vo`; 0 for any other address. */
static int uri_path(char *uri, char *dst)
{
    if (!mem_eq(uri, "file://", 7)) return 0;
    char *p = uri + 7;
    if (mem_eq(p, "localhost/", 10)) p += 9;
    if (*p != '/') return 0;
    long k = 0;
    for (; *p && *p != '?' && *p != '#' && k < PATH_CAP - 1; p++) {
        int c = (unsigned char)*p;
        if (c == '%' && p[1] && p[2]) {
            int hi = p[1] <= '9' ? p[1] - '0' : (p[1] | 32) - 'a' + 10, lo = p[2] <= '9' ? p[2] - '0' : (p[2] | 32) - 'a' + 10;
            if (hi >= 0 && hi < 16 && lo >= 0 && lo < 16) {
                c = hi * 16 + lo;
                p += 2;
            }
        }
        if (!c) return 0;
        dst[k++] = (char)c;
    }
    dst[k] = 0;
    return 1;
}

/* A path as a `file://` address, percent-encoded, appended to dst; the length added. */
static long path_uri(char *path, char *dst)
{
    static const char hex[] = "0123456789ABCDEF";
    long k = 7;
    mem_copy(dst, "file://", 7);
    for (unsigned char *p = (unsigned char *)path; *p; p++) {
        int c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            dst[k++] = (char)c;
        } else {
            dst[k++] = '%';
            dst[k++] = hex[c >> 4];
            dst[k++] = hex[c & 15];
        }
    }
    dst[k] = 0;
    return k;
}

/* ---- the open documents ---- */

static Doc *doc_by_uri(char *uri)
{
    for (long i = 0; i < ndocs; i++)
        if (cstr_eq(docs[i].uri, uri)) return &docs[i];
    return 0;
}

static Doc *doc_by_path(char *path)
{
    for (long i = 0; i < ndocs; i++)
        if (cstr_eq(docs[i].path, path)) return &docs[i];
    return 0;
}

/* Opens or replaces the document at uri with the JSON string at text. */
static void doc_set(char *uri, char *path, char *text, long version)
{
    Doc *d = doc_by_uri(uri), old = {0};
    if (d) {
        old = *d;
    } else {
        if (ndocs == MAX_DOCS) return; /* more open documents than the server follows: not checked */
        d = &docs[ndocs++];
    }
    long nu = cstr_len(uri) + 1, np = cstr_len(path) + 1, nt = js_skip(text, 0) - text;
    d->map_len = nu + np + PATH_CAP + nt + 1;
    d->map = pages_new(d->map_len);
    d->uri = d->map;
    d->path = d->uri + nu;
    d->entry = d->path + np;
    d->text = d->entry + PATH_CAP;
    mem_copy(d->uri, uri, nu);
    mem_copy(d->path, path, np);
    if (old.map) mem_copy(d->entry, old.entry, PATH_CAP);
    d->len = js_unescape(text, d->text);
    d->version = version;
    d->dirty = 1;
    pages_free(old.map, old.map_len);
}

static void all_dirty(void)
{
    for (long i = 0; i < ndocs; i++) docs[i].dirty = 1;
    dirty = 1;
    change_ms = sys_clock_ms();
}

/* ---- the copy: reading through the editor, positions, output ---- */

static long overlay(char *path, char **out)
{
    Doc *d = doc_by_path(path);
    if (!d) return -1;
    *out = d->text;
    return d->len;
}

static void sb_int(SB *sb, long v)
{
    char t[24];
    sb_bytes(sb, t, fmt_int(t, v));
}

/* The text of a file a record names: the editor's, the loaded one, or the disk's; 0 when unreadable. */
static char *text_of(Str *path, long *len)
{
    static Str *last;
    static char *last_text;
    static long last_len;
    if (!last || !str_eq(last, path)) {
        Doc *d = doc_by_path(path->data);
        long f = d ? -1 : file_find(path);
        last = path;
        if (d) {
            last_text = d->text;
            last_len = d->len;
        } else if (f >= 0) {
            last_text = file_get((int)f)->text;
            last_len = file_get((int)f)->len;
        } else {
            last_len = read_whole_file(path->data, &last_text);
            if (last_len < 0) last_text = 0;
        }
    }
    *len = last_len;
    return last_text;
}

/* The byte offset where 0-based line `line` starts (the end of the text when there is none). */
static long line_at(char *text, long len, long line)
{
    long at = 0;
    while (line > 0 && at < len) {
        if (text[at++] == '\n') line--;
    }
    return at;
}

static int cp_len(unsigned char c)
{
    return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
}

/* The protocol's character for the toolchain's column (1-based, in code points) on the line at `at`. */
static long col_to_char(char *text, long len, long at, long col)
{
    long units = 0;
    for (long k = 1; k < col && at < len && text[at] != '\n' && !(text[at] == '\r' && at + 1 < len && text[at + 1] == '\n'); k++) {
        int n = cp_len((unsigned char)text[at]);
        units += utf8_positions ? n : n == 4 ? 2 : 1;
        at += n;
    }
    return units;
}

/* The toolchain's column for the protocol's character on the line at `at`. */
static long char_to_col(char *text, long len, long at, long character)
{
    long units = 0, col = 1;
    while (units < character && at < len && text[at] != '\n' && !(text[at] == '\r' && at + 1 < len && text[at + 1] == '\n')) {
        int n = cp_len((unsigned char)text[at]);
        units += utf8_positions ? n : n == 4 ? 2 : 1;
        at += n;
        col++;
    }
    return col;
}

static long int_of(char *v)
{
    long n = 0, neg = v && *v == '-';
    if (!v) return 0;
    for (v += neg; *v >= '0' && *v <= '9' && n < (1L << 40); v++) n = n * 10 + (*v - '0');
    return neg ? -n : n;
}

/* `{"line":L,"character":C}` for a record's `{"line":..,"col":..}` in file. */
static void sb_position(SB *o, Str *file, char *pos)
{
    long len, line = int_of(js_get(pos, "line")), col = int_of(js_get(pos, "col"));
    char *text = text_of(file, &len);
    if (line < 1) line = 1;
    sb_cstr(o, "{\"line\":");
    sb_int(o, line - 1);
    sb_cstr(o, ",\"character\":");
    sb_int(o, text ? col_to_char(text, len, line_at(text, len, line - 1), col) : col - 1);
    sb_cstr(o, "}");
}

/* `{"start":..,"end":..}` from a record's `start` and `end`. */
static void sb_range(SB *o, Str *file, char *rec)
{
    sb_cstr(o, "{\"start\":");
    sb_position(o, file, js_get(rec, "start"));
    sb_cstr(o, ",\"end\":");
    sb_position(o, file, js_get(rec, "end"));
    sb_cstr(o, "}");
}

/* The JSON value at v as it is written. */
static void sb_raw(SB *o, char *v)
{
    sb_bytes(o, v, js_skip(v, 0) - v);
}

/* The JSON string at v, unescaped. */
static Str *str_of(char *v)
{
    char *d = alloc_bytes(js_skip(v, 0) - v + 1);
    long n = js_unescape(v, d);
    return str_new(d, n);
}

/* A file's address as a JSON string: the editor's own for an open document. */
static void sb_uri(SB *o, Str *path)
{
    Doc *d = doc_by_path(path->data);
    if (d) {
        json_str_into(o, str_from_cstr(d->uri));
        return;
    }
    char *u = alloc_bytes(path->len * 3 + 8);
    long n = path_uri(path->data, u);
    json_str_into(o, str_new(u, n));
}

/* Appends one framed message. */
static void sb_frame(SB *o, SB *body)
{
    sb_cstr(o, "Content-Length: ");
    sb_int(o, body->len);
    sb_cstr(o, "\r\n\r\n");
    sb_bytes(o, body->ptr, body->len);
}

static SB *answer_begin(void)
{
    SB *b = sb_new();
    sb_cstr(b, "{\"jsonrpc\":\"2.0\",\"id\":");
    sb_bytes(b, id, id_len);
    sb_cstr(b, ",\"result\":");
    return b;
}

/* The lines of the captured output as records, each NUL-terminated; calls each(record, kind). */
static Vec *records(SB *cap)
{
    Vec *v = vec_new();
    sb_byte(cap, 0);
    for (char *p = cap->ptr, *end = cap->ptr + cap->len - 1; p < end;) {
        char *nl = p;
        while (nl < end && *nl != '\n') nl++;
        *nl = 0;
        if (*p == '{' && js_skip(p, 0)) vec_push(v, p);
        p = nl + 1;
    }
    return v;
}

static int kind_is(char *rec, char *kind)
{
    char *k = js_get(rec, "kind");
    return k && js_is(k, kind, cstr_len(kind));
}

/* The request's position in the toolchain's form `FILE:LINE:COL`. */
static char *target_word(void)
{
    char *pos = js_get(rq_params, "position");
    long line = int_of(js_get(pos, "line")), character = int_of(js_get(pos, "character")), len = 0;
    Str *path = str_from_cstr(rq_path);
    char *text = text_of(path, &len);
    if (line < 0) line = 0;
    if (character < 0) character = 0;
    SB *sb = sb_new();
    sb_cstr(sb, rq_path);
    sb_byte(sb, ':');
    sb_int(sb, line + 1);
    sb_byte(sb, ':');
    sb_int(sb, text ? char_to_col(text, len, line_at(text, len, line), character) : character + 1);
    return sb_finish(sb)->data;
}

/* ---- the copy: one translator per request ---- */

static void note(char *tag, char *path)
{
    write_all(3, tag, cstr_len(tag));
    write_all(3, path, cstr_len(path));
    write_all(3, "\n", 1);
}

/* One `Diagnostic` from a diagnostic record. */
static void sb_diagnostic(SB *o, Str *file, char *rec)
{
    char *code = js_get(rec, "code"), *sev = js_get(rec, "severity");
    sb_cstr(o, "{\"range\":");
    sb_range(o, file, rec);
    sb_cstr(o, sev && js_is(sev, "warning", 7) ? ",\"severity\":2" : ",\"severity\":1");
    if (code && *code == '"') {
        sb_cstr(o, ",\"code\":");
        sb_raw(o, code);
        sb_cstr(o, ",\"codeDescription\":{\"href\":\"" SITE);
        sb_bytes(o, code + 1, js_skip(code, 0) - code - 2);
        sb_cstr(o, "/\"}");
    }
    sb_cstr(o, ",\"source\":\"vo\",\"message\":");
    sb_raw(o, js_get(rec, "message") ? js_get(rec, "message") : "\"\"");
    sb_cstr(o, "}");
}

/* A diagnostic record's file when it is one the editor can show (a path), else 0. */
static Str *shown_file(char *rec)
{
    char *f = js_get(rec, "file");
    if (!kind_is(rec, "diagnostic") || !f || *f != '"' || f[1] != '/' || !js_get(rec, "start")) return 0;
    return str_of(f);
}

static int has(Vec *paths, Str *p)
{
    for (long i = 0; i < paths->n; i++)
        if (str_eq(vec_get(paths, i), p)) return 1;
    return 0;
}

/* Diagnostics for every file of the entry's program that is open, has some, or had some. */
static void finish_diagnostics(Vec *recs, SB *o)
{
    long f = rq_doc ? file_find(str_from_cstr(rq_doc->path)) : -1;
    if (rq_doc && (f < 0 || !file_get((int)f)->ast)) { /* this program does not hold the document */
        note("N", "");
        return;
    }
    Vec *files = vec_new();
    for (long i = 0; i < ndocs; i++) { /* the open documents the program loaded */
        Str *p = str_from_cstr(docs[i].path);
        long id_ = file_find(p);
        if (id_ >= 0 && file_get((int)id_)->ast) {
            vec_push(files, p);
            note("L", docs[i].path);
        }
    }
    long loaded = files->n, elen = cstr_len(rq_entry);
    for (long i = 0; i < recs->n; i++) {
        Str *p = shown_file(vec_get(recs, i));
        if (p && !has(files, p)) vec_push(files, p);
    }
    long with = files->n;
    for (char *l = pub.p, *end = pub.p + pub.len; l && l < end;) { /* published before, for this entry */
        char *nl = l;
        while (*nl != '\n') nl++;
        if (nl - l > elen && mem_eq(l, rq_entry, elen) && l[elen] == '\t') {
            Str *p = str_new(l + elen + 1, nl - l - elen - 1);
            if (!has(files, p)) vec_push(files, p);
        }
        l = nl + 1;
    }
    for (long k = 0; k < files->n; k++) {
        Str *file = vec_get(files, k);
        Doc *d = doc_by_path(file->data);
        SB *b = sb_new();
        sb_cstr(b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
        sb_uri(b, file);
        if (d) {
            sb_cstr(b, ",\"version\":");
            sb_int(b, d->version);
        }
        sb_cstr(b, ",\"diagnostics\":[");
        long n = 0;
        for (long i = 0; i < recs->n; i++) {
            Str *p = shown_file(vec_get(recs, i));
            if (!p || !str_eq(p, file)) continue;
            if (n++) sb_byte(b, ',');
            sb_diagnostic(b, file, vec_get(recs, i));
        }
        sb_cstr(b, "]}}");
        sb_frame(o, b);
        if (n && k >= loaded && k < with) note("P", file->data); /* an unopened file: to be cleared later */
        if (n && k < loaded) note("P", file->data);
    }
}

/* `{"line":..,"character":..}` at v as two numbers; 0 when it is not one. */
static int read_position(char *v, long *line, long *character)
{
    char *l = js_get(v, "line"), *c = js_get(v, "character");
    if (!l || !c) return 0;
    *line = int_of(l);
    *character = int_of(c);
    return 1;
}

/* Quick fixes: the fixes of the diagnostics that touch the asked range. */
static void finish_actions(Vec *recs, SB *o)
{
    SB *b = answer_begin();
    char *range = js_get(rq_params, "range");
    long sl, sc, el, ec, n = 0;
    Str *doc = str_from_cstr(rq_path);
    sb_byte(b, '[');
    if (read_position(js_get(range, "start"), &sl, &sc) && read_position(js_get(range, "end"), &el, &ec)) {
        for (long i = 0; i < recs->n; i++) {
            char *rec = vec_get(recs, i), *fixes = js_get(rec, "fixes");
            Str *file = shown_file(rec);
            if (!file || !str_eq(file, doc) || !fixes || *fixes != '[') continue;
            long len;
            char *text = text_of(file, &len), *s = js_get(rec, "start"), *e = js_get(rec, "end");
            if (!text) continue;
            long dl = int_of(js_get(s, "line")) - 1, dc = col_to_char(text, len, line_at(text, len, dl), int_of(js_get(s, "col")));
            long fl = int_of(js_get(e, "line")) - 1, fc = col_to_char(text, len, line_at(text, len, fl), int_of(js_get(e, "col")));
            if (dl > el || (dl == el && dc > ec) || fl < sl || (fl == sl && fc < sc)) continue; /* apart */
            for (char *fix = js_ws(fixes + 1); *fix == '{';) {
                char *ff = js_get(fix, "file"), *rep = js_get(fix, "replacement");
                if (ff && *ff == '"' && rep && *rep == '"') {
                    Str *fixfile = str_of(ff), *r = str_of(rep);
                    char *fs = js_get(fix, "start"), *fe = js_get(fix, "end");
                    int empty = int_of(js_get(fs, "line")) == int_of(js_get(fe, "line")) && int_of(js_get(fs, "col")) == int_of(js_get(fe, "col"));
                    SB *title = sb_new(); /* "Insert `;`", "Replace with `str(1)`", "Remove it" */
                    sb_cstr(title, !r->len ? "Remove it" : empty ? "Insert `" : "Replace with `");
                    for (long k = 0; k < r->len && k < 60 && r->data[k] != '\n'; k++) sb_byte(title, r->data[k]);
                    if (r->len) sb_cstr(title, r->len > 60 ? "...`" : "`");
                    if (n++) sb_byte(b, ',');
                    sb_cstr(b, "{\"title\":");
                    json_str_into(b, sb_finish(title));
                    sb_cstr(b, ",\"kind\":\"quickfix\",\"diagnostics\":[");
                    sb_diagnostic(b, file, rec);
                    sb_cstr(b, "],\"edit\":{\"changes\":{");
                    sb_uri(b, fixfile);
                    sb_cstr(b, ":[{\"range\":");
                    sb_range(b, fixfile, fix);
                    sb_cstr(b, ",\"newText\":");
                    sb_raw(b, rep);
                    sb_cstr(b, "}]}}}");
                }
                fix = js_ws(js_skip(fix, 0));
                if (*fix == ',') fix = js_ws(fix + 1);
            }
        }
    }
    sb_cstr(b, "]}");
    sb_frame(o, b);
}

/* The first record of a kind, or 0. */
static char *first(Vec *recs, char *kind)
{
    for (long i = 0; i < recs->n; i++)
        if (kind_is(vec_get(recs, i), kind)) return vec_get(recs, i);
    return 0;
}

/* A record's `file` when it is a path the editor can open, else 0. */
static Str *path_in(char *rec)
{
    char *f = rec ? js_get(rec, "file") : 0;
    return f && *f == '"' && f[1] == '/' ? str_of(f) : 0;
}

/* The JSON values at a and b are written the same (both present). */
static int same_raw(char *a, char *b)
{
    if (!a || !b) return 0;
    long n = js_skip(a, 0) - a;
    return n == js_skip(b, 0) - b && mem_eq(a, b, n);
}

static void finish_type_at(Vec *recs, SB *o)
{
    SB *b = answer_begin();
    char *rec = first(recs, "type_at"), *decl = rec ? js_get(rec, "declaration") : 0;
    Str *file = path_in(rec), *declfile = path_in(decl);
    if (!rec || !file) {
        sb_cstr(b, "null");
    } else if (rq_kind == K_HOVER) {
        char *type = js_get(rec, "type"), *text = js_get(rec, "text"), *doc = js_get(rec, "doc");
        char *sig = doc ? js_get(doc, "signature") : 0, *desc = doc ? js_get(doc, "description") : 0;
        SB *md = sb_new();
        sb_cstr(md, "```vo\n");
        if (sig && *sig == '"') {
            sb_str(md, str_of(sig));
        } else {
            if (text && *text == '"') sb_str(md, str_of(text));
            if (type && *type == '"') {
                sb_cstr(md, ": ");
                sb_str(md, str_of(type));
            }
        }
        sb_cstr(md, "\n```");
        int at_decl = decl && declfile && str_eq(file, declfile) && same_raw(js_get(rec, "start"), js_get(decl, "start"));
        if (sig && *sig == '"' && type && *type == '"' && text && *text == '"' && !at_decl) { /* a call or a use: its type too */
            Str *t = str_of(type), *s = str_of(sig);
            if (!str_eq(t, s)) {
                sb_cstr(md, "\n\nType: `");
                sb_str(md, t);
                sb_cstr(md, "`");
            }
        }
        if (desc && *desc == '"' && desc[1] != '"') {
            sb_cstr(md, "\n\n");
            sb_str(md, str_of(desc));
        }
        sb_cstr(b, "{\"contents\":{\"kind\":\"markdown\",\"value\":");
        json_str_into(b, sb_finish(md));
        sb_cstr(b, "},\"range\":");
        sb_range(b, file, rec);
        sb_cstr(b, "}");
    } else if (!declfile) { /* a builtin or a standard-library item: nowhere to go, nothing to rename */
        sb_cstr(b, "null");
    } else if (rq_kind == K_DEFINITION) {
        sb_cstr(b, "{\"uri\":");
        sb_uri(b, declfile);
        sb_cstr(b, ",\"range\":");
        sb_range(b, declfile, decl);
        sb_cstr(b, "}");
    } else {
        sb_range(b, file, rec);
    }
    sb_cstr(b, "}");
    sb_frame(o, b);
}

static void finish_refs(Vec *recs, SB *o)
{
    SB *b = answer_begin();
    char *ctx = js_get(rq_params, "context"), *incl = ctx ? js_get(ctx, "includeDeclaration") : 0;
    int with_decl = incl && *incl == 't';
    long n = 0;
    sb_byte(b, '[');
    for (long i = 0; i < recs->n; i++) {
        char *rec = vec_get(recs, i), *role = js_get(rec, "role");
        Str *file = kind_is(rec, "ref") ? path_in(rec) : 0;
        if (!file || (!with_decl && role && js_is(role, "declaration", 11))) continue;
        if (n++) sb_byte(b, ',');
        sb_cstr(b, "{\"uri\":");
        sb_uri(b, file);
        sb_cstr(b, ",\"range\":");
        sb_range(b, file, rec);
        sb_cstr(b, "}");
    }
    sb_cstr(b, "]}");
    sb_frame(o, b);
}

/* The edits of `vo edit rename --dry-run`, by file; the toolchain's refusal as the request's error. */
static void finish_rename(Vec *recs, SB *o)
{
    Vec *files = vec_new();
    for (long i = 0; i < recs->n; i++) {
        Str *file = kind_is(vec_get(recs, i), "edit") ? path_in(vec_get(recs, i)) : 0;
        if (file && !has(files, file)) vec_push(files, file);
    }
    SB *b;
    if (!files->n) {
        char *err = first(recs, "toolchain_error"), *m = err ? js_get(err, "message") : 0;
        b = sb_new();
        sb_cstr(b, "{\"jsonrpc\":\"2.0\",\"id\":");
        sb_bytes(b, id, id_len);
        sb_cstr(b, ",\"error\":{\"code\":-32803,\"message\":");
        if (m && *m == '"') sb_raw(b, m);
        else sb_cstr(b, "\"This cannot be renamed.\"");
        sb_cstr(b, "}}");
        sb_frame(o, b);
        return;
    }
    b = answer_begin();
    sb_cstr(b, "{\"changes\":{");
    for (long k = 0; k < files->n; k++) {
        Str *file = vec_get(files, k);
        long n = 0;
        if (k) sb_byte(b, ',');
        sb_uri(b, file);
        sb_cstr(b, ":[");
        for (long i = 0; i < recs->n; i++) {
            char *rec = vec_get(recs, i);
            Str *f = kind_is(rec, "edit") ? path_in(rec) : 0;
            if (!f || !str_eq(f, file) || !js_get(rec, "replacement")) continue;
            if (n++) sb_byte(b, ',');
            sb_cstr(b, "{\"range\":");
            sb_range(b, file, rec);
            sb_cstr(b, ",\"newText\":");
            sb_raw(b, js_get(rec, "replacement"));
            sb_cstr(b, "}");
        }
        sb_byte(b, ']');
    }
    sb_cstr(b, "}}}");
    sb_frame(o, b);
}

/* One edit over the lines that differ between the editor's text and what `vo fmt` would write. */
static void finish_format(SB *o)
{
    SB *b = answer_begin();
    long olen = 0;
    char *old = text_of(str_from_cstr(rq_path), &olen);
    Str *new = write_captured;
    if (!new || !old) {
        sb_cstr(b, "[]}");
        sb_frame(o, b);
        return;
    }
    long a = 0, line = 0, oe = olen, ne = new->len; /* whole lines in common at the start, then at the end */
    for (long i = 0; i < olen && i < new->len && old[i] == new->data[i]; i++) {
        if (old[i] == '\n') {
            a = i + 1;
            line++;
        }
    }
    while (oe > a && ne > a && old[oe - 1] == new->data[ne - 1]) {
        oe--;
        ne--;
    }
    while (oe < olen && old[oe - 1 < 0 ? 0 : oe - 1] != '\n' && !(oe == a)) { /* back to a line boundary */
        oe++;
        ne++;
    }
    long end_line = line, end_char = 0;
    for (long i = a; i < oe; i++)
        if (old[i] == '\n') end_line++;
    if (oe > a && old[oe - 1] != '\n') { /* the text has no final newline: the edit ends inside the last line */
        long ls = oe;
        while (ls > a && old[ls - 1] != '\n') ls--;
        for (long i = ls; i < oe;) {
            int n = cp_len((unsigned char)old[i]);
            end_char += utf8_positions ? n : n == 4 ? 2 : 1;
            i += n;
        }
    }
    sb_cstr(b, "[{\"range\":{\"start\":{\"line\":");
    sb_int(b, line);
    sb_cstr(b, ",\"character\":0},\"end\":{\"line\":");
    sb_int(b, end_line);
    sb_cstr(b, ",\"character\":");
    sb_int(b, end_char);
    sb_cstr(b, "}},\"newText\":");
    json_str_into(b, str_new(new->data + a, ne - a));
    sb_cstr(b, "}]}");
    sb_frame(o, b);
}

/* `{"line":..,"character":..}` for a byte offset in a file's text. */
static void sb_offset(SB *o, char *text, long len, long off)
{
    long line = 0, start = 0, units = 0;
    if (off > len) off = len;
    for (long i = 0; i < off; i++) {
        if (text[i] == '\n') {
            line++;
            start = i + 1;
        }
    }
    for (long i = start; i < off;) {
        int n = cp_len((unsigned char)text[i]);
        units += utf8_positions ? n : n == 4 ? 2 : 1;
        i += n;
    }
    sb_cstr(o, "{\"line\":");
    sb_int(o, line);
    sb_cstr(o, ",\"character\":");
    sb_int(o, units);
    sb_cstr(o, "}");
}

static void sb_symbol(SB *b, FileRec *f, Str *name, long kind, Node *whole, long sel_start, long sel_end)
{
    sb_cstr(b, "{\"name\":");
    json_str_into(b, name && name->len ? name : str_from_cstr("?"));
    sb_cstr(b, ",\"kind\":");
    sb_int(b, kind);
    sb_cstr(b, ",\"range\":{\"start\":");
    sb_offset(b, f->text, f->len, whole->start);
    sb_cstr(b, ",\"end\":");
    sb_offset(b, f->text, f->len, whole->end);
    sb_cstr(b, "},\"selectionRange\":{\"start\":");
    sb_offset(b, f->text, f->len, sel_start);
    sb_cstr(b, ",\"end\":");
    sb_offset(b, f->text, f->len, sel_end);
    sb_cstr(b, "}");
}

/* The outline: the file's top-level items, records and variants with their members. */
static void finish_symbols(SB *o)
{
    SB *b = answer_begin();
    long id_ = load_file(str_from_cstr(rq_path)), n = 0;
    FileRec *f = id_ >= 0 ? file_get((int)id_) : 0;
    sb_byte(b, '[');
    Vec *items = f && f->ast ? f->ast->a.v : 0;
    for (long i = 0; items && i < items->n; i++) {
        Node *it = vec_get(items, i);
        long kind = 0;
        if (!it) continue;
        switch (it->kind) {
        case N_FN: kind = 12; break;      /* Function */
        case N_RECORD: kind = 23; break;  /* Struct */
        case N_VARIANT: kind = 10; break; /* Enum */
        case N_CONST: kind = 14; break;   /* Constant */
        case N_EFFECTS: kind = 11; break; /* Interface */
        case N_TEST: kind = 6; break;     /* Method */
        default: break;
        }
        if (!kind || it->end <= it->start) continue;
        if (n++) sb_byte(b, ',');
        if (it->kind == N_TEST) {
            sb_symbol(b, f, it->a.s, kind, it, it->start, it->start + 4);
        } else {
            Node *name = it->a.n;
            if (!name) {
                n--;
                if (n) b->len--; /* the comma */
                continue;
            }
            sb_symbol(b, f, name->a.s, kind, it, name->start, name->end);
            if ((it->kind == N_RECORD || it->kind == N_VARIANT) && it->b.v) {
                long m = 0;
                sb_cstr(b, ",\"children\":[");
                for (long k = 0; k < it->b.v->n; k++) {
                    Node *mem = vec_get(it->b.v, k);
                    if (!mem || (mem->kind != N_FIELD_DECL && mem->kind != N_CASE_DECL) || !mem->a.n) continue;
                    if (m++) sb_byte(b, ',');
                    sb_symbol(b, f, mem->a.n->a.s, mem->kind == N_FIELD_DECL ? 8 : 22, mem, mem->a.n->start, mem->a.n->end);
                    sb_byte(b, '}');
                }
                sb_byte(b, ']');
            }
        }
        sb_byte(b, '}');
    }
    sb_cstr(b, "]}");
    sb_frame(o, b);
}

/* lsp_finish in a copy: the command has ended; its captured output becomes the answer. */
static int finish(int code)
{
    static SB *scratch;
    SB *cap = out_capture, *o = sb_new();
    (void)code;
    lsp_finish = 0;
    scratch = sb_new();
    out_capture = scratch; /* whatever is printed from here on is dropped */
    Vec *recs = records(cap);
    switch (rq_kind) {
    case K_DIAG: finish_diagnostics(recs, o); break;
    case K_PROBE: { /* does this program hold the request's file? */
        long f = file_find(str_from_cstr(rq_path));
        note(f >= 0 && file_get((int)f)->ast ? "L" : "N", "");
        break;
    }
    case K_ACTION: finish_actions(recs, o); break;
    case K_HOVER:
    case K_DEFINITION:
    case K_PREPARE: finish_type_at(recs, o); break;
    case K_REFS: finish_refs(recs, o); break;
    case K_RENAME: finish_rename(recs, o); break;
    case K_FORMAT: finish_format(o); break;
    default: finish_symbols(o); break;
    }
    write_all(1, o->ptr, o->len);
    return 0;
}

/* The copy after fork: never returns. */
static void copy_main(int out_fd, int notes_fd, long parent) NORETURN;
static void copy_main(int out_fd, int notes_fd, long parent)
{
    static char w_vo[] = "vo", w_json[] = "--json", w_check[] = "check", w_query[] = "query", w_type_at[] = "type-at",
                w_refs[] = "refs", w_edit[] = "edit", w_dry[] = "--dry-run", w_rename[] = "rename", w_fmt[] = "fmt";
    char *av[10];
    long ac = 0;
    vo_syscall2(SYS_setpgid, 0, 0);
    sys_die_with_parent(parent);
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&old_mask, 0, 8);
    if (out_fd == 3) out_fd = (int)vo_syscall3(SYS_fcntl, out_fd, 0 /* F_DUPFD */, 10);
    sys_dup_to(notes_fd, 3);
    sys_dup_to(out_fd, 1);
    sys_close(0);
    if (vo_syscall3(SYS_close_range, 4, ~0UL, 0) < 0)
        for (int fd = 4; fd < 4096; fd++) sys_close(fd);
    read_overlay = overlay;
    write_capture = 1;
    out_capture = sb_new();
    lsp_finish = finish;
    av[ac++] = w_vo;
    switch (rq_kind) {
    case K_DIAG:
    case K_PROBE:
    case K_ACTION:
        av[ac++] = w_check, av[ac++] = w_json, av[ac++] = rq_entry;
        break;
    case K_HOVER:
    case K_DEFINITION:
    case K_PREPARE:
        av[ac++] = w_query, av[ac++] = w_json, av[ac++] = w_type_at, av[ac++] = rq_entry, av[ac++] = target_word();
        break;
    case K_REFS:
        av[ac++] = w_query, av[ac++] = w_json, av[ac++] = w_refs, av[ac++] = rq_entry, av[ac++] = target_word();
        break;
    case K_RENAME: {
        char *name = js_get(rq_params, "newName");
        Str *n = str_of(name);
        if (n->len != cstr_len(n->data) || (n->data[0] == '-' && n->data[1] == '-')) n = str_from_cstr("?"); /* refused as a name */
        av[ac++] = w_edit, av[ac++] = w_json, av[ac++] = w_dry, av[ac++] = w_rename, av[ac++] = rq_entry;
        av[ac++] = target_word(), av[ac++] = n->data;
        break;
    }
    case K_FORMAT:
        av[ac++] = w_fmt, av[ac++] = w_json, av[ac++] = rq_path;
        break;
    default: /* the outline needs no command */
        vo_exit(0);
    }
    av[ac] = 0;
    char **env = (char **)envp;
    for (; env && *env; env++) { /* a test's way to a slow request (tests/lsp.py) */
        char *pos = js_get(rq_params, "position");
        if (rq_kind == K_HOVER && cstr_eq(*env, "VO_LSP_TEST=slow") && pos && int_of(js_get(pos, "line")) == 4242) {
            long ts[2] = {30, 0};
            vo_syscall5(SYS_ppoll, 0, 0, (long)ts, 0, 8);
        }
    }
    vo_exit(vo_main(ac, av, (char **)envp));
    for (;;) {}
}

/* ---- the server: input ---- */

static void in_fill(int wait)
{
    if (in_pos) {
        for (long i = in_pos; i < inb.len; i++) inb.p[i - in_pos] = inb.p[i];
        inb.len -= in_pos;
        in_scan = in_scan > in_pos ? in_scan - in_pos : 0;
        in_pos = 0;
    }
    buf_need(&inb, 65536);
    for (;;) {
        long n = sys_read(0, inb.p + inb.len, inb.cap - inb.len - 1);
        if (n == -E_INTR) continue;
        if (n == -E_AGAIN) {
            if (!wait) return;
            sys_wait_fd(0, POLLIN, -1);
            continue;
        }
        if (n <= 0) in_eof = 1;
        else inb.len += n;
        return;
    }
}

/* The frame at offset `at`: 1 with its body's offset and length, 0 when it is not all there yet, -1
 * when what is there cannot be a frame. */
static int frame_at(long at, long *body, long *len)
{
    long end = -1, n = -1;
    for (long i = at; i + 3 < inb.len && i < at + 4096; i++) {
        if (mem_eq(inb.p + i, "\r\n\r\n", 4)) {
            end = i;
            break;
        }
    }
    if (end < 0) return inb.len - at >= 4096 ? -1 : 0;
    for (long i = at; i < end;) {
        static const char key[] = "content-length:";
        long k = 0;
        while (k < 15 && i + k < end && (inb.p[i + k] | 32) == key[k]) k++;
        if (k == 15) {
            long j = i + 15;
            while (j < end && inb.p[j] == ' ') j++;
            if (j < end && inb.p[j] >= '0' && inb.p[j] <= '9') {
                n = 0;
                while (j < end && inb.p[j] >= '0' && inb.p[j] <= '9' && n <= MSG_LIMIT) n = n * 10 + (inb.p[j++] - '0');
            }
        }
        while (i < end && inb.p[i] != '\n') i++;
        i++;
    }
    if (n < 0 || n > MSG_LIMIT) return -1;
    *body = end + 4;
    *len = n;
    return inb.len >= end + 4 + n;
}

/* While a request runs: among the frames not looked at yet, a cancellation of it is taken out. */
static int cancel_scan(void)
{
    int cancelled = 0;
    long body, len;
    while (frame_at(in_scan, &body, &len) == 1) {
        char keep = inb.p[body + len], *m, *p;
        inb.p[body + len] = 0;
        p = js_ws(inb.p + body);
        char *end = *p == '{' ? js_skip(p, 0) : 0;
        if (end && !*js_ws(end) && !js_get(p, "id") && (m = js_get(p, "method")) && js_is(m, "$/cancelRequest", 15)) {
            char *which = js_get(js_get(p, "params"), "id");
            if (which && js_skip(which, 0) - which == id_len && mem_eq(which, id, id_len)) {
                cancelled = 1;
                for (long i = body; i < body + len; i++) inb.p[i] = ' ';
            }
        }
        inb.p[body + len] = keep;
        in_scan = body + len;
    }
    return cancelled;
}

/* ---- the server: running a copy ---- */

static void drain(int *fd, Buf *b, int *over)
{
    for (;;) {
        buf_need(b, 65536);
        long n = sys_read(*fd, b->p + b->len, 65536);
        if (n == -E_INTR) continue;
        if (n == -E_AGAIN) return;
        if (n <= 0) {
            sys_close(*fd);
            *fd = -1;
            return;
        }
        b->len += n;
        if (b->len > MSG_LIMIT) *over = 1;
    }
}

/* Runs the request (rq_*) in a copy. On R_OK `out` holds its framed messages and `notes` its notes. */
static int run_copy(Buf *out)
{
    int po[2] = {-1, -1}, pn[2] = {-1, -1};
    long pid = -1;
    notes.len = 0;
    if (sys_pipe(po) >= 0 && sys_pipe(pn) >= 0) {
        long parent = sys_getpid();
        pid = sys_fork();
        if (pid == 0) copy_main(po[1], pn[1], parent);
    }
    if (po[1] >= 0) sys_close(po[1]);
    if (pn[1] >= 0) sys_close(pn[1]);
    if (pid < 0) {
        if (po[0] >= 0) sys_close(po[0]);
        if (pn[0] >= 0) sys_close(pn[0]);
        return R_FAILED;
    }
    vo_syscall3(SYS_fcntl, po[0], F_SETFL, O_NONBLOCK);
    vo_syscall3(SYS_fcntl, pn[0], F_SETFL, O_NONBLOCK);
    in_scan = in_pos;
    long deadline = sys_clock_ms() + LIMIT_MS;
    char **env = (char **)envp;
    for (; env && *env; env++)
        if (cstr_eq(*env, "VO_LSP_TEST=slow")) deadline -= LIMIT_MS - 400; /* (tests do not wait five seconds) */
    int status = 0, reaped = 0, why = R_OK, killed = 0, over = 0, gone = 0;
    if (rq_kind != K_DIAG && cancel_scan()) why = R_CANCELLED;
    while (!reaped) {
        long left = deadline - sys_clock_ms();
        if (left <= 0 && why == R_OK) why = R_FAILED;
        if (over && why == R_OK) why = R_FAILED;
        if (why != R_OK && !killed) {
            vo_syscall2(SYS_kill, -pid, 9);
            sys_kill(pid);
            killed = 1;
        }
        PollFd fds[5];
        int nf = 0, at_in = -1;
        fds[nf++] = (PollFd){sig_fd, POLLIN, 0};
        fds[nf++] = (PollFd){1, 0, 0};
        if (po[0] >= 0) fds[nf++] = (PollFd){po[0], POLLIN, 0};
        if (pn[0] >= 0) fds[nf++] = (PollFd){pn[0], POLLIN, 0};
        if (!in_eof && why == R_OK) fds[at_in = nf++] = (PollFd){0, POLLIN, 0};
        long ts[2] = {(left > 0 ? left : 100) / 1000, (left > 0 ? left : 100) % 1000 * 1000000};
        long r = vo_syscall5(SYS_ppoll, (long)fds, nf, (long)ts, 0, 8);
        if (r < 0 && r != -E_INTR) break;
        if (po[0] >= 0) drain(&po[0], out, &over);
        if (pn[0] >= 0) drain(&pn[0], &notes, &over);
        if (r > 0 && (fds[1].revents & (POLLERR | POLLHUP))) gone = 1, why = R_FAILED;
        if (r > 0 && at_in >= 0 && fds[at_in].revents) {
            in_fill(0);
            if (rq_kind == K_DIAG) why = R_ABORTED; /* something new: the newest text wins */
            else if (cancel_scan()) why = R_CANCELLED;
        }
        char info[128];
        while (sys_read(sig_fd, info, sizeof info) > 0) {}
        reaped = vo_syscall4(SYS_wait4, pid, (long)&status, WNOHANG, 0) == pid;
    }
    vo_syscall2(SYS_kill, -pid, 9);
    if (!reaped) sys_wait(pid, &status);
    if (po[0] >= 0) drain(&po[0], out, &over);
    if (pn[0] >= 0) drain(&pn[0], &notes, &over);
    if (po[0] >= 0) sys_close(po[0]);
    if (pn[0] >= 0) sys_close(pn[0]);
    if (gone) leave(1);
    if (why == R_OK && status != 0) why = R_FAILED;
    return why;
}

/* ---- the server: diagnostics ---- */

static void publish_empty(char *uri_json)
{
    Buf b = {0};
    buf_str(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    buf_str(&b, uri_json);
    buf_str(&b, ",\"diagnostics\":[]}}");
    send_body(&b);
    buf_drop(&b);
}

static void warn_once(void)
{
    if (warned++) return;
    Buf b = {0};
    buf_str(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"window/showMessage\",\"params\":{\"type\":2,\"message\":"
                "\"vo could not check a file within 5 seconds; its diagnostics may be out of date.\"}}");
    send_body(&b);
    buf_drop(&b);
}

/* Drops the published lines of an entry; with `clear`, tells the editor those files are clean. */
static void pub_remove(char *entry, int clear)
{
    Buf kept = {0};
    long elen = cstr_len(entry);
    for (char *l = pub.p, *end = pub.p + pub.len; l && l < end;) {
        char *nl = l;
        while (*nl != '\n') nl++;
        if (nl - l > elen && mem_eq(l, entry, elen) && l[elen] == '\t') {
            if (clear) {
                char path[PATH_CAP], uri[PATH_CAP * 3 + 16];
                long n = nl - l - elen - 1;
                if (n < PATH_CAP) {
                    mem_copy(path, l + elen + 1, n);
                    path[n] = 0;
                    uri[0] = '"';
                    long k = path_uri(path, uri + 1);
                    uri[k + 1] = '"';
                    uri[k + 2] = 0;
                    if (!doc_by_path(path)) publish_empty(uri);
                }
            }
        } else {
            buf_add(&kept, l, nl - l + 1);
        }
        l = nl + 1;
    }
    buf_drop(&pub);
    pub = kept;
}

/* Checks the program at `entry` for document d. 1: it holds d and is published; 0: it does not hold d;
 * -1: the check was overtaken by new input; -2: it failed. */
static int diag_run(Doc *d, char *entry)
{
    Buf out = {0};
    rq_kind = K_DIAG;
    rq_doc = d;
    rq_entry = entry;
    rq_params = 0;
    int r = run_copy(&out), held = 1;
    if (r == R_OK) {
        Buf lines = {0};
        buf_add(&lines, notes.p, notes.len);
        buf_add(&lines, "", 1);
        if (lines.p[0] == 'N') {
            held = 0;
        } else {
            send_all(out.p, out.len);
            pub_remove(entry, 0);
            for (char *l = lines.p; *l;) {
                char *nl = l;
                while (*nl && *nl != '\n') nl++;
                char keep = *nl;
                *nl = 0;
                Doc *o = *l == 'L' ? doc_by_path(l + 1) : 0;
                if (o) {
                    o->dirty = 0;
                    if (cstr_len(entry) < PATH_CAP) mem_copy(o->entry, entry, cstr_len(entry) + 1);
                } else if (*l == 'P') {
                    buf_str(&pub, entry);
                    buf_str(&pub, "\t");
                    buf_str(&pub, l + 1);
                    buf_str(&pub, "\n");
                }
                l = keep ? nl + 1 : nl;
            }
        }
        buf_drop(&lines);
    }
    buf_drop(&out);
    if (r == R_ABORTED) return -1;
    if (r != R_OK) {
        warn_once();
        d->dirty = 0;
        return -2;
    }
    return held;
}

static int exists(char *path)
{
    long fd = sys_open(path, O_PATH | O_CLOEXEC);
    if (fd >= 0) sys_close((int)fd);
    return fd >= 0 || doc_by_path(path);
}

/* The next entry file to try for the file at path (research R7): the nearest `main.vo` at or above
 * it, then the ones further up as far as the workspace folder, then the file itself. cand holds the
 * one tried last ("" at first); 0 when none is left. */
static int next_candidate(char *path, char *cand)
{
    char top[PATH_CAP];
    long n = cstr_len(path), rl = cstr_len(root), c = cstr_len(cand);
    if (n >= PATH_CAP - 16 || cstr_eq(cand, path)) return c ? 0 : (mem_copy(cand, path, n + 1), 1);
    long dir = n;
    while (dir > 0 && path[dir - 1] != '/') dir--; /* the file's directory, with its slash */
    if (rl && dir > rl && mem_eq(path, root, rl) && path[rl] == '/') {
        mem_copy(top, root, rl + 1);
    } else { /* outside the workspace folder: only its own directory */
        rl = dir > 1 ? dir - 1 : dir;
        mem_copy(top, path, rl);
        top[rl] = 0;
    }
    long at = dir; /* the directory to look in next */
    if (c) {
        at = c - 7; /* cand is DIR/main.vo: one directory up */
        if (at <= rl + 1) at = 0;
        else {
            at--;
            while (at > 0 && cand[at - 1] != '/') at--;
        }
    }
    for (; at > rl; ) {
        mem_copy(cand, path, at);
        mem_copy(cand + at, "main.vo", 8);
        if (cstr_eq(cand, path) || exists(cand)) return 1;
        if (at <= rl + 1) break;
        at--;
        while (at > 0 && cand[at - 1] != '/') at--;
    }
    mem_copy(cand, path, n + 1);
    return 1;
}

/* Finds d's program and publishes its diagnostics. 0 when overtaken by new input. */
static int check_doc(Doc *d)
{
    char cand[PATH_CAP];
    int r;
    if (d->entry[0]) { /* the program it was in last time */
        mem_copy(cand, d->entry, PATH_CAP);
        if ((r = diag_run(d, cand)) != 0) return r != -1;
        d->entry[0] = 0;
    }
    cand[0] = 0;
    while (next_candidate(d->path, cand))
        if ((r = diag_run(d, cand)) != 0) return r != -1;
    return 1;
}

static void run_checks(void)
{
    dirty = 0;
    for (long i = 0; i < ndocs; i++) {
        if (!docs[i].dirty) continue;
        if (!check_doc(&docs[i])) { /* new input arrived: handle it, then check again */
            dirty = 1;
            change_ms = sys_clock_ms();
            return;
        }
    }
}

/* ---- the server: messages ---- */

/* Answers a request about a document by running it in a copy. */
static void request(int kind, char *params, char *empty)
{
    char *td = js_get(params, "textDocument"), *uri = td ? js_get(td, "uri") : 0, *pos = js_get(params, "position");
    char *range = js_get(params, "range"), *name = js_get(params, "newName");
    int positional = kind == K_HOVER || kind == K_DEFINITION || kind == K_PREPARE || kind == K_REFS || kind == K_RENAME;
    if (!uri || *uri != '"' || js_skip(uri, 0) - uri >= PATH_CAP ||
        (positional && (!pos || *pos != '{' || !js_get(pos, "line") || !js_get(pos, "character"))) ||
        (kind == K_ACTION && (!range || *range != '{')) || (kind == K_RENAME && (!name || *name != '"'))) {
        reply_error(-32602, "The request's parameters do not have the shape this method needs.");
        return;
    }
    js_unescape(uri, rq_uri);
    if (!uri_path(rq_uri, rq_path)) { /* not a file: nothing to say */
        reply(empty);
        return;
    }
    Buf out = {0};
    static char entry[PATH_CAP];
    int r = R_OK;
    rq_doc = doc_by_path(rq_path);
    rq_params = params;
    if (rq_doc && rq_doc->entry[0]) {
        mem_copy(entry, rq_doc->entry, PATH_CAP);
    } else { /* not checked yet, or not open: find its program first */
        entry[0] = 0;
        rq_kind = K_PROBE;
        rq_entry = entry;
        while (r == R_OK && next_candidate(rq_path, entry)) {
            if (cstr_eq(entry, rq_path)) break;
            out.len = 0;
            r = run_copy(&out);
            if (r == R_OK && notes.len && notes.p[0] == 'L') break;
        }
        if (rq_doc && r == R_OK) mem_copy(rq_doc->entry, entry, PATH_CAP);
        out.len = 0;
    }
    rq_kind = kind;
    rq_entry = entry;
    if (r == R_OK) r = run_copy(&out);
    if (r == R_OK && out.len) send_all(out.p, out.len);
    else if (r == R_CANCELLED) reply_error(-32800, "The request was cancelled.");
    else reply_error(-32803, "vo could not answer this request (it failed or took more than 5 seconds).");
    buf_drop(&out);
}

static void did_open_or_change(char *params, int change)
{
    char *td = js_get(params, "textDocument"), *uri = td ? js_get(td, "uri") : 0, *text = td ? js_get(td, "text") : 0;
    char *version = td ? js_get(td, "version") : 0;
    if (change) { /* the whole text, in the last change */
        char *changes = js_get(params, "contentChanges"), *k, *v;
        text = 0;
        for (char *c = changes && *changes == '[' ? js_ws(changes + 1) : "]"; *c == '{';) {
            char *p = js_ws(c + 1);
            while (js_member(&p, &k, &v))
                if (js_is(k, "text", 4)) text = v;
            c = js_ws(js_skip(c, 0));
            if (*c == ',') c = js_ws(c + 1);
        }
    }
    if (!uri || *uri != '"' || !text || *text != '"' || js_skip(uri, 0) - uri >= PATH_CAP) return;
    js_unescape(uri, rq_uri);
    if (!uri_path(rq_uri, rq_path)) return;
    if (change && !doc_by_uri(rq_uri)) return;
    doc_set(rq_uri, rq_path, text, int_of(version));
    all_dirty();
}

static void did_close(char *params)
{
    char *td = js_get(params, "textDocument"), *uri = td ? js_get(td, "uri") : 0;
    if (!uri || *uri != '"' || js_skip(uri, 0) - uri >= PATH_CAP) return;
    js_unescape(uri, rq_uri);
    Doc *d = doc_by_uri(rq_uri);
    if (!d) return;
    char entry[PATH_CAP];
    mem_copy(entry, d->entry, PATH_CAP);
    pages_free(d->map, d->map_len);
    *d = docs[--ndocs];
    char keep = *js_skip(uri, 0);
    *js_skip(uri, 0) = 0;
    publish_empty(uri);
    uri[cstr_len(uri)] = keep;
    int used = 0; /* its program is no longer watched: what was published for its other files is cleared */
    for (long i = 0; i < ndocs; i++) used |= cstr_eq(docs[i].entry, entry);
    if (entry[0] && !used) pub_remove(entry, 1);
    all_dirty();
}

static void initialize(char *params)
{
    char *folders = js_get(params, "workspaceFolders"), *uri = js_get(params, "rootUri");
    char *caps = js_get(params, "capabilities"), *general = caps ? js_get(caps, "general") : 0;
    char *enc = general ? js_get(general, "positionEncodings") : 0;
    if (folders && *folders == '[' && *js_ws(folders + 1) == '{') uri = js_get(js_ws(folders + 1), "uri");
    root[0] = 0;
    if (uri && *uri == '"' && js_skip(uri, 0) - uri < PATH_CAP) {
        js_unescape(uri, rq_uri);
        if (uri_path(rq_uri, root)) {
            long n = cstr_len(root);
            while (n > 1 && root[n - 1] == '/') root[--n] = 0;
        }
    }
    utf8_positions = 0;
    for (char *e = enc && *enc == '[' ? js_ws(enc + 1) : "]"; *e == '"';) {
        if (js_is(e, "utf-8", 5)) utf8_positions = 1;
        e = js_ws(js_skip(e, 0));
        if (*e == ',') e = js_ws(e + 1);
    }
    long vn = 0;
    while (vo_version_text[3 + vn] != ' ') vn++; /* "vo X.Y.Z schema 1" */
    Buf b = {0};
    buf_str(&b, "{\"capabilities\":{\"positionEncoding\":\"");
    buf_str(&b, utf8_positions ? "utf-8" : "utf-16");
    buf_str(&b, "\",\"textDocumentSync\":{\"openClose\":true,\"change\":1,\"save\":{\"includeText\":false}},"
                "\"hoverProvider\":true,\"definitionProvider\":true,\"referencesProvider\":true,"
                "\"renameProvider\":{\"prepareProvider\":true},\"documentFormattingProvider\":true,"
                "\"documentSymbolProvider\":true,\"codeActionProvider\":{\"codeActionKinds\":[\"quickfix\"]}},"
                "\"serverInfo\":{\"name\":\"vo\",\"version\":\"");
    buf_add(&b, (char *)vo_version_text + 3, vn);
    buf_str(&b, "\"}}");
    buf_add(&b, "", 1);
    reply(b.p);
    buf_drop(&b);
    initialized = 1;
}

static void handle(char *msg)
{
    char *p = js_ws(msg), *end;
    id_len = 0;
    if (!*p) return; /* empty, or a cancellation that was taken out */
    if (!(end = js_skip(p, 0)) || *js_ws(end)) {
        reply_error(-32700, "The message is not one JSON value.");
        return;
    }
    char *idv = js_get(p, "id"), *m = js_get(p, "method"), *params = js_get(p, "params");
    if (*p != '{') {
        reply_error(-32600, "A message must be one JSON object.");
        return;
    }
    if (!m) return; /* an answer: the server asks nothing that needs one */
    long idn = idv ? js_skip(idv, 0) - idv : 0;
    if (*m != '"' || idn > (long)sizeof id) {
        reply_error(-32600, "A request needs a string method and a string or number id.");
        return;
    }
#define IS(name) js_is(m, name, sizeof name - 1)
    if (!idv) { /* a notification */
        if (IS("exit")) leave(shutdown_seen ? 0 : 1);
        if (!initialized) return;
        if (IS("textDocument/didOpen")) did_open_or_change(params, 0);
        else if (IS("textDocument/didChange")) did_open_or_change(params, 1);
        else if (IS("textDocument/didClose")) did_close(params);
        else if (IS("textDocument/didSave")) all_dirty(); /* other files may have been saved too */
        return;
    }
    mem_copy(id, idv, idn);
    id_len = idn;
    if (IS("initialize")) initialize(params);
    else if (!initialized) reply_error(-32002, "The server has not been initialized: send initialize first.");
    else if (IS("shutdown")) shutdown_seen = 1, reply("null");
    else if (IS("textDocument/hover")) request(K_HOVER, params, "null");
    else if (IS("textDocument/definition")) request(K_DEFINITION, params, "null");
    else if (IS("textDocument/references")) request(K_REFS, params, "[]");
    else if (IS("textDocument/prepareRename")) request(K_PREPARE, params, "null");
    else if (IS("textDocument/rename")) request(K_RENAME, params, "null");
    else if (IS("textDocument/formatting")) request(K_FORMAT, params, "[]");
    else if (IS("textDocument/documentSymbol")) request(K_SYMBOLS, params, "[]");
    else if (IS("textDocument/codeAction")) request(K_ACTION, params, "[]");
    else reply_error(-32601, "This server does not offer that method.");
#undef IS
}

int cmd_lsp(void)
{
    unsigned long mask = (1UL << (SIGCHLD - 1)) | (1UL << (SIGPIPE - 1)), chld = 1UL << (SIGCHLD - 1);
    vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&mask, (long)&old_mask, 8);
    sig_fd = (int)vo_syscall4(SYS_signalfd4, -1, (long)&chld, 8, O_NONBLOCK | O_CLOEXEC);
    for (;;) {
        long body, len;
        int f = frame_at(in_pos, &body, &len);
        if (f < 0) return 1; /* not the protocol: nothing sensible can follow */
        if (f == 1) {
            char *msg = pages_new(len + 1); /* its own copy: input that arrives meanwhile moves the buffer */
            mem_copy(msg, inb.p + body, len);
            in_pos = body + len;
            handle(msg);
            pages_free(msg, len + 1);
            continue;
        }
        if (in_eof) return 1; /* the editor went away without the closing exchange */
        if (dirty) {
            long left = change_ms + PAUSE_MS - sys_clock_ms();
            if (left <= 0) {
                run_checks();
                continue;
            }
            if (!sys_wait_fd(0, POLLIN, left)) continue;
        }
        in_fill(1);
    }
}
