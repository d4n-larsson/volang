/* Source file table, diagnostics (collect, sort, cap, render), summaries, toolchain errors (ported
 * from src/diag.s, spec 008). */
#include "vo.h"

FileRec file_table[MAX_FILES];
long file_count;
Vec *te_related;     /* spec 006: vec of item declarations shown as `related` by toolchain_error */
long cur_gen;        /* spec 006: files of older generations are invisible to file_find */
Diag **diag_arr;
long diag_n;
static long diag_cap;
long diag_errors;
long diag_warnings;
long diag_truncated;
long diag_mute;      /* 1: diagnostics are built but not recorded (scanning other files) */
static Fix **emitted_fixes; /* fixes already shown */
static long emitted_n;

/* Registers a source file; builds its line table and directory. Returns the file id. */
int file_add(Str *path, char *text, long len)
{
    long id = file_count++;
    FileRec *f = &file_table[id];
    f->path = path;
    f->text = text;
    f->len = len;
    f->gen = cur_gen; /* spec 006: the generation it belongs to (vo edit's re-check) */
    f->tokens = 0;
    f->std = 0;
    f->pkg = 0;
    f->ast = 0;
    long lines = 1; /* count lines */
    for (long i = 0; i < len; i++)
        if (text[i] == '\n') lines++;
    f->nlines = lines;
    uint32_t *starts = alloc(lines * 4);
    f->lines = starts;
    starts[0] = 0;
    long k = 1;
    for (long i = 0; i < len; i++)
        if (text[i] == '\n') starts[k++] = (uint32_t)(i + 1);
    long n = path->len; /* directory part: up to and including the last '/' */
    while (n > 0 && path->data[n - 1] != '/') n--;
    f->dir = str_new(path->data, n);
    return (int)id;
}

FileRec *file_get(int id)
{
    return &file_table[id];
}

/* 1-based line of a byte offset. */
long pos_line(int file, long off)
{
    FileRec *f = &file_table[file];
    long lo = 0, hi = f->nlines; /* find the last line start <= off */
    while (lo + 1 < hi) {
        long mid = (lo + hi) >> 1;
        if ((unsigned long)f->lines[mid] > (unsigned long)off)
            hi = mid;
        else
            lo = mid;
    }
    return lo + 1;
}

long line_start(int file, long line)
{
    return file_table[file].lines[line - 1];
}

/* Offset of the newline that ends the line (or the file length). */
long line_end(int file, long line)
{
    FileRec *f = &file_table[file];
    if (line >= f->nlines) return f->len;
    return (long)f->lines[line] - 1;
}

/* 1-based column, counted in code points. */
long pos_col(int file, long off)
{
    long start = line_start(file, pos_line(file, off));
    return cp_count(file_get(file)->text + start, off - start) + 1;
}

/* Forgets every diagnostic (spec 006: vo edit checks the program a second time). */
void diag_reset(void)
{
    diag_n = 0;
    diag_errors = 0;
    diag_warnings = 0;
    diag_truncated = 0;
    emitted_n = 0;
}

/* The byte offset of a 1-based line and column (columns in code points), the inverse of pos_line
 * and pos_col; -1 when the line or column is outside the file. Column = line length + 1 is the end of
 * the line. */
long pos_offset(int file, long line, long col)
{
    if (line <= 0 || col <= 0) return -1;
    if (line > file_get(file)->nlines) return -1;
    long end = line_end(file, line);
    long p = line_start(file, line);
    char *text = file_get(file)->text;
    for (long skip = col - 1; skip; skip--) {
        if (p >= end) return -1;
        p++; /* skip one code point: its continuation bytes too */
        while (p < end && ((unsigned char)text[p] & 0xc0) == 0x80) p++;
    }
    return p;
}

/* Spec 031: while a template, pattern, statement or route that came from a constant is checked, every
 * new diagnostic spans the constant's use and names its value, and gets no fix (offsets inside the
 * value are not places in the source). */
Node *diag_const_node;

/* " (`name` is "value")", or " (its value is "value")" for a folded expression. */
static Str *const_note(Node *n)
{
    SB *sb = sb_new();
    if (n->b.n && n->b.n->kind == N_CONST && n->b.n->a.n) {
        sb_cstr(sb, " (`");
        sb_str(sb, n->b.n->a.n->a.s);
        sb_cstr(sb, "` is ");
    } else {
        sb_cstr(sb, " (its value is ");
    }
    render_str_lit(sb, n->a.s);
    sb_byte(sb, ')');
    return sb_finish(sb);
}

Diag *diag_add(char *code, int sev, int file, long start, long end, Str *message)
{
    if (diag_const_node) {
        Node *n = diag_const_node;
        file = (int)n->file;
        start = n->start;
        end = n->end;
        if (message && message->len && message->data[message->len - 1] == '.') /* before the full stop */
            message = str_concat(str_new(message->data, message->len - 1), str_concat(const_note(n), str_from_cstr(".")));
        else if (message)
            message = str_concat(message, const_note(n));
    }
    Diag *d = alloc(D_SIZE);
    d->code = code;
    d->sev = sev;
    d->file = file;
    d->start = start;
    d->end = end;
    d->msg = message;
    if (diag_mute) return d;
    d->seq = diag_n;
    if (diag_n >= diag_cap) { /* grow the pointer array */
        long cap = diag_cap * 2;
        if (!cap) cap = 64;
        Diag **arr = alloc(cap * 8);
        mem_copy((char *)arr, (char *)diag_arr, diag_n * 8);
        diag_arr = arr;
        diag_cap = cap;
    }
    diag_arr[diag_n++] = d;
    if (sev)
        diag_warnings++;
    else
        diag_errors++;
    return d;
}

void diag_set_ef(Diag *d, Str *expected, Str *found)
{
    d->expected = expected;
    d->found = found;
}

/* W0601: the names in scope, a vec of {name str, type str} pairs, innermost first. */
void diag_set_scope(Diag *d, Vec *v)
{
    d->scope = v;
}

int ranges_touch(long s1, long e1, long s2, long e2)
{
    return !(s1 > e2 || s2 > e1);
}

/* Appends a fix unless it overlaps or touches one the diagnostic already has. */
void diag_add_fix(Diag *d, int file, long start, long end, Str *replacement)
{
    if (diag_const_node) return; /* spec 031: not a place in the source */
    Fix **link = (Fix **)&d->fixes.p;
    for (; *link; link = &(*link)->next)
        if (ranges_touch((*link)->start, (*link)->end, start, end)) return;
    Fix *f = alloc(FIX_SIZE);
    f->next = 0;
    f->file = file;
    f->start = start;
    f->end = end;
    f->text = replacement;
    *link = f;
}

void diag_add_related(Diag *d, int file, long start, long end, Str *message)
{
    Fix **link = (Fix **)&d->related.p;
    while (*link) link = &(*link)->next;
    Fix *f = alloc(FIX_SIZE);
    f->next = 0;
    f->file = file;
    f->start = start;
    f->end = end;
    f->text = message;
    *link = f;
}

/* Sort order: file, start, insertion order. */
int diag_before(Diag *a, Diag *b)
{
    if (a->file != b->file) return a->file < b->file;
    if (a->start != b->start) return a->start < b->start;
    return a->seq < b->seq;
}

/* Insertion sort of the diagnostic array. */
void diag_sort(void)
{
    for (long i = 1; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        long j = i - 1;
        while (j >= 0 && diag_before(d, diag_arr[j])) {
            diag_arr[j + 1] = diag_arr[j];
            j--;
        }
        diag_arr[j + 1] = d;
    }
}

void json_pos(SB *sb, int file, long off)
{
    sb_cstr(sb, "{\"line\":");
    int_to_dec(sb, pos_line(file, off));
    sb_cstr(sb, ",\"col\":");
    int_to_dec(sb, pos_col(file, off));
    sb_byte(sb, '}');
}

/* Writes "file":"...","start":{..},"end":{..} (without braces around it). */
void json_span(SB *sb, int file, long start, long end)
{
    json_str_into(sb, file_get(file)->path);
    sb_cstr(sb, ",\"start\":");
    json_pos(sb, file, start);
    sb_cstr(sb, ",\"end\":");
    json_pos(sb, file, end);
}

/* The source text of a span as a JSON string. */
void json_src_text(SB *sb, int file, long start, long end)
{
    char *text = file_get(file)->text;
    sb_byte(sb, '"');
    json_escape_into(sb, text + start, end - start);
    sb_byte(sb, '"');
}

/* 1 when the fix overlaps or touches a fix of an earlier diagnostic; otherwise records it. */
int fix_already_shown(Fix *fix)
{
    for (long i = 0; i < emitted_n; i++) {
        Fix *e = emitted_fixes[i];
        if (e->file == fix->file && ranges_touch(e->start, e->end, fix->start, fix->end)) return 1;
    }
    if (!emitted_fixes) emitted_fixes = alloc(8 * 4096); /* the 50 shown diagnostics x 64 fixes */
    if (emitted_n < 4096) emitted_fixes[emitted_n++] = fix;
    return 0;
}

/* Drops fixes that overlap a fix of an earlier (already emitted) diagnostic. */
void diag_filter_fixes(Diag *d)
{
    Fix **link = (Fix **)&d->fixes.p;
    while (*link) {
        if (fix_already_shown(*link))
            *link = (*link)->next; /* unlink */
        else
            link = &(*link)->next;
    }
}

void diag_json(SB *sb, Diag *d)
{
    sb_cstr(sb, "{\"v\":1,\"kind\":\"diagnostic\",\"code\":\"");
    sb_cstr(sb, (char *)d->code);
    sb_cstr(sb, "\",\"name\":\"");
    sb_str(sb, catalog_name((char *)d->code));
    sb_cstr(sb, d->sev ? "\",\"severity\":\"warning\",\"file\":" : "\",\"severity\":\"error\",\"file\":");
    json_span(sb, (int)d->file, d->start, d->end);
    sb_cstr(sb, ",\"text\":");
    json_src_text(sb, (int)d->file, d->start, d->end);
    sb_cstr(sb, ",\"message\":");
    json_str_into(sb, d->msg);
    if (d->expected) {
        sb_cstr(sb, ",\"expected\":");
        json_str_into(sb, d->expected);
    }
    if (d->found) {
        sb_cstr(sb, ",\"found\":");
        json_str_into(sb, d->found);
    }
    if (d->scope) {
        sb_cstr(sb, ",\"scope\":[");
        for (long i = 0; i < d->scope->n; i++) {
            W *pair = vec_get(d->scope, i);
            sb_cstr(sb, i ? ",{\"name\":" : "{\"name\":");
            json_str_into(sb, pair[0].s);
            sb_cstr(sb, ",\"type\":");
            json_str_into(sb, pair[1].s);
            sb_byte(sb, '}');
        }
        sb_byte(sb, ']');
    }
    sb_cstr(sb, ",\"fixes\":[");
    int first = 1;
    for (Fix *f = d->fixes.p; f; f = f->next) {
        if (!first) sb_byte(sb, ',');
        first = 0;
        sb_cstr(sb, "{\"file\":");
        json_span(sb, (int)f->file, f->start, f->end);
        sb_cstr(sb, ",\"replacement\":");
        json_str_into(sb, f->text);
        sb_byte(sb, '}');
    }
    sb_cstr(sb, "],\"related\":[");
    first = 1;
    for (Fix *f = d->related.p; f; f = f->next) {
        if (!first) sb_byte(sb, ',');
        first = 0;
        sb_cstr(sb, "{\"file\":");
        json_span(sb, (int)f->file, f->start, f->end);
        sb_cstr(sb, ",\"message\":");
        json_str_into(sb, f->text);
        sb_byte(sb, '}');
    }
    sb_cstr(sb, "]}\n");
}

/* Writes path:line:col. */
void human_loc(SB *sb, int file, long off)
{
    sb_str(sb, file_get(file)->path);
    sb_byte(sb, ':');
    int_to_dec(sb, pos_line(file, off));
    sb_byte(sb, ':');
    int_to_dec(sb, pos_col(file, off));
}

/* Appends str with newlines and tabs shown as \n and \t. */
void sb_visible(SB *sb, Str *str)
{
    GC_ROOT(&str); /* it may be new (a removal's text) */
    for (long i = 0; i < str->len; i++) {
        int c = (unsigned char)str->data[i];
        if (c == '\n' || c == '\t') {
            sb_byte(sb, '\\');
            sb_byte(sb, c == '\n' ? 'n' : 't');
        } else {
            sb_byte(sb, c);
        }
    }
    GC_UNROOT(1);
}

/* The source line of start with a caret line under the span. */
void human_snippet(SB *sb, int file, long start, long end)
{
    long line = pos_line(file, start);
    long ls = line_start(file, line);
    long le = line_end(file, line);
    char *text = file_get(file)->text;
    long pad = line < 10 ? 3 : line < 100 ? 2 : line < 1000 ? 1 : 0; /* right-aligned to 4 columns */
    for (; pad; pad--) sb_byte(sb, ' ');
    int_to_dec(sb, line);
    sb_cstr(sb, " | ");
    sb_bytes(sb, text + ls, le - ls);
    sb_byte(sb, '\n');
    sb_cstr(sb, "     | ");
    for (long i = ls; i < start; i++) { /* caret line: copy tabs, spaces elsewhere */
        int c = (unsigned char)text[i];
        if ((c & 0xC0) == 0x80) continue; /* continuation byte: no column */
        sb_byte(sb, c == '\t' ? '\t' : ' ');
    }
    if ((unsigned long)end > (unsigned long)le) end = le; /* at most to the end of the line */
    long carets = cp_count(text + start, end - start);
    if (!carets) carets = 1;
    for (; carets; carets--) sb_byte(sb, '^');
    sb_byte(sb, '\n');
}

void diag_human(SB *sb, Diag *d)
{
    human_loc(sb, (int)d->file, d->start);
    sb_cstr(sb, d->sev ? ": warning[" : ": error[");
    sb_cstr(sb, (char *)d->code);
    sb_cstr(sb, "]: ");
    sb_str(sb, d->msg);
    sb_byte(sb, '\n');
    human_snippet(sb, (int)d->file, d->start, d->end);
    if (d->scope) {
        sb_cstr(sb, "    in scope: ");
        if (d->scope->n == 0) sb_cstr(sb, "nothing");
        for (long i = 0; i < d->scope->n; i++) {
            if (i) {
                sb_byte(sb, ',');
                sb_byte(sb, ' ');
            }
            W *pair = vec_get(d->scope, i);
            sb_str(sb, pair[0].s);
            sb_byte(sb, ':');
            sb_byte(sb, ' ');
            sb_str(sb, pair[1].s);
        }
        sb_byte(sb, '\n');
    }
    if (d->expected && d->found) {
        sb_cstr(sb, "  expected `");
        sb_str(sb, d->expected);
        sb_cstr(sb, "`, found `");
        sb_str(sb, d->found);
        sb_cstr(sb, "`\n");
    }
    for (Fix *f = d->fixes.p; f; f = f->next) {
        if (f->text->len == 0)
            sb_cstr(sb, "  fix: remove `");
        else if (f->start == f->end)
            sb_cstr(sb, "  fix: insert `");
        else
            sb_cstr(sb, "  fix: replace with `");
        Str *shown = f->text;
        if (f->text->len == 0) /* removal: show the removed text */
            shown = str_new(file_get((int)f->file)->text + f->start, f->end - f->start);
        sb_visible(sb, shown);
        sb_cstr(sb, "`\n");
    }
    for (Fix *r = d->related.p; r; r = r->next) {
        sb_cstr(sb, "  note: ");
        human_loc(sb, (int)r->file, r->start);
        sb_byte(sb, ':');
        sb_byte(sb, ' ');
        sb_str(sb, r->text);
        sb_byte(sb, '\n');
    }
}

/* Spec 013 (L7): human output shows control bytes from hostile source (escape sequences, a lone CR)
 * as \xNN instead of sending them to the terminal; tab, newline and CR before LF stay.
 * ponytail: the caret line still counts an escaped byte as one column */
SB *sb_escape_controls(SB *sb)
{
    long n = sb->len, k = 0;
    for (long i = 0; i < n; i++) {
        int c = (unsigned char)sb->ptr[i];
        k += (c < 0x20 && c != '\t' && c != '\n' && !(c == '\r' && i + 1 < n && sb->ptr[i + 1] == '\n')) || c == 0x7f;
    }
    if (!k) return sb;
    GC_ROOT(&sb);
    SB *out = sb_new();
    GC_ROOT(&out);
    for (long i = 0; i < n; i++) {
        int c = (unsigned char)sb->ptr[i];
        if ((c < 0x20 && c != '\t' && c != '\n' && !(c == '\r' && i + 1 < n && sb->ptr[i + 1] == '\n')) || c == 0x7f) {
            sb_cstr(out, "\\x");
            sb_byte(out, "0123456789abcdef"[c >> 4]);
            sb_byte(out, "0123456789abcdef"[c & 15]);
        } else {
            sb_byte(out, c);
        }
    }
    GC_UNROOT(2);
    return out;
}

/* Sorts, drops overlapping fixes, caps at 50, and writes every diagnostic once. */
void diag_emit_all(int json, int fd)
{
    diag_sort();
    SB *sb = 0;
    GC_ROOT(&sb); /* catalog_name and a removal's text allocate while it is built */
    long i = 0;
    for (; i < diag_n; i++) {
        if (i >= DIAG_CAP_SHOWN) {
            diag_truncated = diag_n - i;
            GC_UNROOT(1);
            return;
        }
        Diag *d = diag_arr[i];
        diag_filter_fixes(d);
        sb = sb_new();
        if (json) {
            diag_json(sb, d);
        } else {
            diag_human(sb, d);
            sb = sb_escape_controls(sb);
        }
        out_bytes(fd, sb->ptr, sb->len);
    }
    GC_UNROOT(1);
}

/* The number of W0601 diagnostics when command is "check" (spec 005 FR-005), else 0. */
static long holes_for(char *command)
{
    if (command[0] != 'c' || command[1] != 'h') return 0; /* "check": the only command starting "ch" */
    long n = 0;
    for (long i = 0; i < diag_n; i++) {
        const char *code = diag_arr[i]->code;
        if (code[0] == 'W' && code[2] == '6' && code[3] == '0' && code[4] == '1') n++; /* W0601 (spec 018 adds W0602, W0603) */
    }
    return n;
}

/* extra: a str of additional JSON members (starting with ','), or 0. */
void summary_emit(int json, int fd, char *command, long files, Str *extra)
{
    GC_ROOT(&extra); /* often new (the caller's members) */
    SB *sb = sb_new();
    if (json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"summary\",\"command\":\"");
        sb_cstr(sb, command);
        sb_cstr(sb, "\",\"errors\":");
        int_to_dec(sb, diag_errors);
        sb_cstr(sb, ",\"warnings\":");
        int_to_dec(sb, diag_warnings);
        sb_cstr(sb, ",\"truncated\":");
        int_to_dec(sb, diag_truncated);
        sb_cstr(sb, ",\"files\":");
        int_to_dec(sb, files);
        sb_str(sb, extra);
        if (diag_accepted) { /* spec 027: warnings an `@allow` accepted */
            sb_cstr(sb, ",\"accepted\":");
            int_to_dec(sb, diag_accepted);
        }
        long holes = holes_for(command);
        if (holes) {
            sb_cstr(sb, ",\"holes\":");
            int_to_dec(sb, holes);
        }
        sb_cstr(sb, "}\n");
    } else { /* human: "N errors, M warnings" */
        int_to_dec(sb, diag_errors);
        sb_cstr(sb, diag_errors == 1 ? " error" : " errors");
        if (diag_warnings == 1) {
            sb_cstr(sb, ", 1 warning");
        } else {
            sb_cstr(sb, ", ");
            int_to_dec(sb, diag_warnings);
            sb_cstr(sb, " warnings");
        }
        if (diag_accepted) { /* spec 027: " (N accepted)" */
            sb_cstr(sb, " (");
            int_to_dec(sb, diag_accepted);
            sb_cstr(sb, " accepted)");
        }
        long holes = holes_for(command); /* " (N holes)" */
        if (holes) {
            sb_cstr(sb, " (");
            int_to_dec(sb, holes);
            sb_cstr(sb, holes == 1 ? " hole)" : " holes)");
        }
        if (diag_truncated) {
            sb_cstr(sb, " (");
            int_to_dec(sb, diag_truncated);
            sb_cstr(sb, " more not shown)");
        }
        sb_byte(sb, '\n');
    }
    out_bytes(fd, sb->ptr, sb->len);
    GC_UNROOT(1);
}

/* ,"related":[{"file":..,"start":..,"end":..},...] for the nodes in te_related (a vec), if any. */
static void te_related_json(SB *sb)
{
    if (!te_related) return;
    sb_cstr(sb, ",\"related\":[");
    for (long i = 0; i < te_related->n; i++) {
        if (i) sb_byte(sb, ',');
        Node *item = vec_get(te_related, i);
        Node *name = item->a.n; /* an item: its name node */
        sb_cstr(sb, "{\"file\":");
        json_span(sb, (int)name->file, name->start, name->end);
        sb_byte(sb, '}');
    }
    sb_byte(sb, ']');
}

/* One line per related declaration: "  declared at FILE:LINE:COL". */
static void te_related_human(SB *sb)
{
    if (!te_related) return;
    for (long i = 0; i < te_related->n; i++) {
        Node *name = ((Node *)vec_get(te_related, i))->a.n;
        sb_cstr(sb, "  declared at ");
        human_loc(sb, (int)name->file, name->start);
        sb_byte(sb, '\n');
    }
}

void toolchain_error(int json, int fd, char *code, Str *message, Str *path)
{
    GC_ROOT(&message); /* often new (T0009 at exit) */
    GC_ROOT(&path);
    SB *sb = sb_new();
    GC_ROOT(&sb); /* catalog_name allocates while it is built */
    if (json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"toolchain_error\",\"code\":\"");
        sb_cstr(sb, code);
        sb_cstr(sb, "\",\"name\":\"");
        sb_str(sb, catalog_name(code));
        sb_byte(sb, '"');
        sb_cstr(sb, ",\"message\":");
        json_str_into(sb, message);
        if (path) {
            sb_cstr(sb, ",\"path\":");
            json_str_into(sb, path);
        }
        te_related_json(sb); /* spec 006: related declarations (te_related) */
        sb_cstr(sb, "}\n");
    } else {
        sb_cstr(sb, "error[");
        sb_cstr(sb, code);
        sb_cstr(sb, "]: ");
        sb_str(sb, message);
        sb_byte(sb, '\n');
        te_related_human(sb);
    }
    out_bytes(fd, sb->ptr, sb->len);
    GC_UNROOT(3);
}

/* Edit distance between two byte strings; returns max+1 as soon as it must exceed max. */
long levenshtein(char *a, long na, char *b, long nb, long max)
{
    long diff = na - nb; /* lengths alone differ by more than max */
    if (diff < 0) diff = -diff;
    if (diff > max) return max + 1;
    long *row = alloc((nb + 1) * 8);
    for (long j = 0; j <= nb; j++) row[j] = j;
    for (long i = 1; i <= na; i++) {
        long diag = row[0]; /* row[0] of the previous row */
        row[0] = i;
        long rowmin = i;
        for (long j = 1; j <= nb; j++) {
            long above = row[j];
            long left = row[j - 1]; /* already updated */
            long v = diag + (a[i - 1] != b[j - 1]); /* substitution */
            if (above + 1 < v) v = above + 1;
            if (left + 1 < v) v = left + 1;
            diag = above; /* next diagonal = old above */
            row[j] = v;
            if (v < rowmin) rowmin = v;
        }
        if (rowmin > max) return max + 1; /* every path already exceeds max */
    }
    return row[nb] <= max ? row[nb] : max + 1;
}

/* Edit distance between two strs, capped at 3 (only 0-2 matter for suggestions). */
long name_distance(Str *a, Str *b)
{
    return levenshtein(a->data, a->len, b->data, b->len, 2);
}

/* The most recently added diagnostic, or 0. */
Diag *diag_last(void)
{
    return diag_n ? diag_arr[diag_n - 1] : 0;
}
