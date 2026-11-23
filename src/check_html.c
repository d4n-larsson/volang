/* Spec 051: `std.html.html(template, values...)`. The template must be a string literal or a
 * constant; it is decoded from its source text (lit_decode, as format's), so diagnostics point inside
 * it. A small tokenizer (research R3) finds each `{}` hole's place: text, a quoted attribute value, or a
 * whole URL attribute value; holes anywhere else are E0338. Tags must nest and close (E0337). The
 * values are typed against their places (E0339), the pieces are stored on the call (`call->e.p`, a Vec
 * of FmtPiece: spec = place * 8 + kind) and the call becomes builtin B_HTML, which the compiler lowers
 * (chtml) to pieces joined and wrapped as an `Html` record. html_piece renders one hole at run time
 * (research R4). Codes E0335-E0340: specs/051-html-templates/contracts/html.md. */
#include "vo.h"

enum { HK_STR = 1, HK_INT, HK_FLOAT, HK_BOOL, HK_HTML, HK_HTML_LIST };
enum { HP_TEXT = 0, HP_ATTR, HP_URL };

static char *hb;  /* the template, decoded */
static long *ho;  /* the source offset of each decoded byte */
static long hn;
static int hfile;
static int herr;  /* an error was reported for this call */
static int hnest; /* a nesting error was reported (no more of them: they would cascade) */
static int hraw;  /* inside script or style: a single brace is the program's own (CSS, JavaScript) */

/* Whether decl is the standard library's std.html declaration of that name. */
static int is_html_decl(Node *decl, char *name)
{
    if (!decl) return 0;
    FileRec *f = file_get((int)decl->file);
    Str *p = f->path;
    if (!f->std || p->len < 7 || !mem_eq(p->data + p->len - 7, "html.vo", 7)) return 0;
    return str_eq_cstr(decl->a.n->a.s, name);
}

int is_html_call(Node *call)
{
    Node *d = call->c.n;
    return d && d->kind == N_FN && is_html_decl(d, "html");
}

/* Whether t is std.html's Html. */
int ty_is_html(Type *t)
{
    return t && t->kind == TY_RECORD && t->decl.n && is_html_decl(t->decl.n, "Html");
}

/* Whether the current file is std/html.vo (where Html's field may be used). */
int in_html_module(Node *at)
{
    FileRec *f = file_get((int)at->file);
    return f->std && f->path->len >= 7 && mem_eq(f->path->data + f->path->len - 7, "html.vo", 7);
}

#define M_FILLED "Holes take the values in order and have nothing inside: write `{}` (format a number with `format` before inserting it)."
#define M_BRACE "A brace in a template is written `{{` or `}}`; a hole is `{}`."

static Diag *herr_at(char *code, long a, long b, Str *m)
{
    herr = 1;
    if (b <= a) b = a + 1;
    if (b > hn) b = hn;
    if (a >= hn) a = hn > 0 ? hn - 1 : 0;
    return diag_add(code, SEV_ERROR, hfile, ho[a], ho[b], m);
}

static int lower_c(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* Whether the decoded bytes at i start with word, ASCII case-insensitive. */
static int at_word(long i, char *word)
{
    for (long k = 0; word[k]; k++)
        if (i + k >= hn || lower_c((unsigned char)hb[i + k]) != word[k]) return 0;
    return 1;
}

static int is_name_c(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':' ||
           c == '.';
}

static int is_space_c(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int is_hole(long i)
{
    return i + 1 < hn && hb[i] == '{' && hb[i + 1] == '}';
}

static int is_brace_pair(long i)
{
    return i + 1 < hn && (hb[i] == '{' || hb[i] == '}') && hb[i + 1] == hb[i];
}

/* The end of `{...}` with something inside (`{0}`, `{name}`, `{:x}`) at i, or 0. */
static long filled_hole_end(long i)
{
    if (hb[i] != '{') return 0;
    for (long k = i + 1; k < hn && k - i <= 40; k++) {
        char c = hb[k];
        if (c == '}') return k > i + 1 ? k + 1 : 0;
        if (c == '{' || c == '<' || c == '>' || c == '\n' || c == '"' || c == '\'') return 0;
    }
    return 0;
}

/* The name at [a, b) in lower case. */
static Str *lname(long a, long b)
{
    Str *s = str_new(hb + a, b - a);
    for (long i = 0; i < s->len; i++) s->data[i] = (char)lower_c((unsigned char)s->data[i]);
    return s;
}

static int name_in(Str *n, const char *const *list)
{
    for (long i = 0; list[i]; i++)
        if (str_eq_cstr(n, (char *)list[i])) return 1;
    return 0;
}

static const char *const voids[] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source",
                                    "track", "wbr", 0};
static const char *const url_attrs[] = {"href", "src", "action", "formaction", "poster", "cite", 0};

/* The pieces being built: literal text so far, and the holes with their places. */
static Vec *hpieces;
static SB *htext;
static long hholes;

static void flush_text(void)
{
    Str *t = sb_finish(htext);
    if (t->len) vec_push(hpieces, (void *)fmt_new_piece(t, -1, 0));
    htext = sb_new();
}

/* A hole at i in place (or refused with why, place < 0). */
static void hole(long i, long place, char *why, Node *lit)
{
    if (place < 0) {
        Diag *d = herr_at("E0338", i, i + 2,
                          msg("A value cannot go here ($1): no escaping makes it safe. Put the value in text or in a quoted attribute.",
                              str_from_cstr(why), 0, 0));
        (void)d;
        (void)lit;
    }
    flush_text();
    vec_push(hpieces, (void *)fmt_new_piece(0, hholes, place < 0 ? 0 : place));
    hholes++;
}

/* Copies decoded [a, b) to the text, braces undoubled; holes at their place. Returns b. */
static void copy_text(long a, long b, long place, char *why, Node *lit)
{
    for (long i = a; i < b;) {
        if (is_brace_pair(i)) {
            sb_byte(htext, hb[i]);
            i += 2;
        } else if (is_hole(i)) {
            hole(i, place, why, lit);
            i += 2;
        } else if (!hraw && filled_hole_end(i)) {
            long e = filled_hole_end(i);
            herr_at("E0336", i, e, str_from_cstr(M_FILLED));
            hole(i, HP_TEXT, 0, lit);
            i = e;
        } else {
            if (!hraw && (hb[i] == '{' || hb[i] == '}'))
                herr_at("E0336", i, i + 1, str_from_cstr(M_BRACE));
            sb_byte(htext, hb[i]);
            i++;
        }
    }
}

/* Open elements. */
static Str **hstack;
static long hdepth, hcap;
static long *hstack_at;

static void push_el(Str *name, long at)
{
    if (hdepth == hcap) {
        long nc = hcap ? hcap * 2 : 16;
        Str **ns = alloc(nc * 8);
        long *na = alloc(nc * 8);
        for (long i = 0; i < hdepth; i++) {
            ns[i] = hstack[i];
            na[i] = hstack_at[i];
        }
        hstack = ns;
        hstack_at = na;
        hcap = nc;
    }
    hstack[hdepth] = name;
    hstack_at[hdepth++] = at;
}

static void nest_err(long a, long b, Str *m)
{
    if (hnest) return;
    hnest = 1;
    herr_at("E0337", a, b, m);
}

/* An end tag `</name>` at [a, b). */
static void end_tag(Str *name, long a, long b)
{
    if (name_in(name, voids)) {
        nest_err(a, b, msg("`<$1>` is a void element: it has no end tag.", name, 0, 0));
        return;
    }
    if (!hdepth) {
        nest_err(a, b, msg("`</$1>` closes no open element.", name, 0, 0));
        return;
    }
    Str *top = hstack[hdepth - 1];
    if (!str_eq(top, name)) {
        nest_err(a, b, msg("`</$1>` closes `<$2>`: elements must be closed in the order they were opened.", name, top, 0));
        return;
    }
    hdepth--;
}

/* The index after a raw-text element's content (script, style: holes refused; textarea, title: holes
 * are text), at its end tag `</name`; the end tag is handled by the caller. */
static long raw_text(long i, Str *name, int escapable, Node *lit)
{
    long start = i;
    while (i < hn && !(hb[i] == '<' && i + 1 < hn && hb[i + 1] == '/' && at_word(i + 2, (char *)name->data) &&
                       (i + 2 + name->len >= hn || !is_name_c((unsigned char)hb[i + 2 + name->len]))))
        i++;
    if (escapable) {
        copy_text(start, i, HP_TEXT, 0, lit);
    } else {
        char why[64];
        long k = 0;
        for (char *p = "inside <"; *p; p++) why[k++] = *p;
        for (long j = 0; j < name->len && k < 50; j++) why[k++] = name->data[j];
        why[k++] = '>';
        why[k] = 0;
        hraw = 1;
        copy_text(start, i, -1, why, lit);
        hraw = 0;
    }
    return i;
}

/* A start tag from `<` at a; returns the index after `>`. */
static long start_tag(long a, Node *lit)
{
    long i = a + 1;
    while (i < hn && is_name_c((unsigned char)hb[i])) i++;
    Str *name = lname(a + 1, i);
    sb_bytes(htext, hb + a, i - a);
    int self_close = 0;
    while (i < hn && hb[i] != '>') {
        if (is_space_c((unsigned char)hb[i])) {
            sb_byte(htext, hb[i++]);
            continue;
        }
        if (hb[i] == '/' && i + 1 < hn && hb[i + 1] == '>') {
            self_close = 1;
            sb_byte(htext, hb[i++]);
            continue;
        }
        if (is_hole(i)) {
            hole(i, -1, "in a tag, where an attribute name goes", lit);
            i += 2;
            continue;
        }
        long ns = i; /* an attribute name */
        while (i < hn && !is_space_c((unsigned char)hb[i]) && hb[i] != '=' && hb[i] != '>' && !is_hole(i) &&
               !(hb[i] == '/' && i + 1 < hn && hb[i + 1] == '>'))
            i++;
        Str *attr = lname(ns, i);
        copy_text(ns, i, -1, "in an attribute name", lit);
        if (is_hole(i)) {
            hole(i, -1, "in an attribute name", lit);
            i += 2;
            continue;
        }
        long eq = i;
        while (eq < hn && is_space_c((unsigned char)hb[eq])) eq++;
        if (eq >= hn || hb[eq] != '=') continue; /* an attribute without a value */
        copy_text(i, eq + 1, HP_TEXT, 0, lit);
        i = eq + 1;
        while (i < hn && is_space_c((unsigned char)hb[i])) sb_byte(htext, hb[i++]);
        int unsafe_attr = (attr->len > 2 && attr->data[0] == 'o' && attr->data[1] == 'n') || str_eq_cstr(attr, "style");
        int url = name_in(attr, url_attrs) || (str_eq_cstr(attr, "data") && str_eq_cstr(name, "object"));
        if (i < hn && (hb[i] == '"' || hb[i] == '\'')) {
            char q = hb[i];
            sb_byte(htext, q);
            long vs = ++i;
            while (i < hn && hb[i] != q) i++;
            long ve = i;
            if (unsafe_attr) {
                char *why = attr->data[0] == 'o' ? "in an event-handler attribute, which runs as a script"
                                                 : "in a style attribute";
                copy_text(vs, ve, -1, why, lit);
            } else if (url) {
                /* the whole URL: its scheme is checked at run time; after a start that fixes the scheme
                   (a `/`, `?`, `#` or `:` in the template's text before the first hole), the rest is
                   attribute text; else a value could still choose the scheme */
                long first = vs;
                while (first < ve && !is_hole(first)) first++;
                int fixed = 0;
                for (long k = vs; k < first; k++) fixed |= hb[k] == '/' || hb[k] == '?' || hb[k] == '#' || hb[k] == ':';
                if (ve - vs == 2 && is_hole(vs))
                    copy_text(vs, ve, HP_URL, 0, lit);
                else if (first == ve || fixed)
                    copy_text(vs, ve, HP_ATTR, 0, lit);
                else
                    copy_text(vs, ve, -1, "where it could choose the URL's scheme: make the value the whole URL, or start the URL in the template (`/notes/{}`)", lit);
            } else {
                copy_text(vs, ve, HP_ATTR, 0, lit);
            }
            if (i < hn) sb_byte(htext, hb[i++]);
        } else { /* an unquoted value */
            long vs = i;
            while (i < hn && !is_space_c((unsigned char)hb[i]) && hb[i] != '>') i++;
            int has = 0;
            for (long k = vs; k + 1 < i; k++) has |= is_hole(k);
            if (has) {
                Diag *d = herr_at("E0338", vs, i,
                                  str_from_cstr("A value cannot go in an unquoted attribute value: quote the value."));
                Node *lit_n = lit;
                if (!(lit_n->flags & NF_CONST)) {
                    char *src = file_get(hfile)->text;
                    int raw = src[lit_n->start] == '`';
                    SB *fx = sb_new();
                    sb_cstr(fx, raw ? "\"" : "\\\"");
                    sb_bytes(fx, src + ho[vs], ho[i] - ho[vs]);
                    sb_cstr(fx, raw ? "\"" : "\\\"");
                    diag_add_fix(d, hfile, ho[vs], ho[i], sb_finish(fx));
                }
            }
            copy_text(vs, i, has ? HP_ATTR : HP_TEXT, 0, lit);
        }
    }
    if (i >= hn) {
        nest_err(a, i, msg("The tag `<$1` is not closed with `>`.", name, 0, 0));
        return i;
    }
    sb_byte(htext, '>');
    i++;
    if (name_in(name, voids)) return i;
    if (self_close) {
        nest_err(a, i, msg("`<$1/>` does not close in HTML: write `<$1></$1>`.", name, 0, 0));
        return i;
    }
    int raw = str_eq_cstr(name, "script") || str_eq_cstr(name, "style");
    int esc = str_eq_cstr(name, "textarea") || str_eq_cstr(name, "title");
    if (raw || esc) {
        long e = raw_text(i, name, esc, lit);
        if (e >= hn) {
            nest_err(a, i, msg("`<$1>` is not closed.", name, 0, 0));
            return e;
        }
        long close = e + 2;
        while (close < hn && hb[close] != '>') close++;
        sb_bytes(htext, hb + e, close + 1 - e);
        return close + 1;
    }
    push_el(name, a);
    return i;
}

/* Tokenizes the template, filling hpieces; holes beyond this point only count. */
static void tokenize(Node *lit)
{
    hdepth = 0;
    hraw = 0;
    int seen = 0; /* anything but spaces so far (a doctype must come first) */
    for (long i = 0; i < hn;) {
        char c = hb[i];
        if (c == '<' && at_word(i, "<!--")) {
            long e = i + 4;
            while (e + 2 < hn && !(hb[e] == '-' && hb[e + 1] == '-' && hb[e + 2] == '>')) e++;
            e = e + 3 <= hn ? e + 3 : hn;
            copy_text(i, e, -1, "inside a comment", lit);
            i = e;
            seen = 1;
            continue;
        }
        if (c == '<' && at_word(i, "<!doctype")) {
            long e = i;
            while (e < hn && hb[e] != '>') e++;
            if (seen) nest_err(i, e + 1, str_from_cstr("`<!DOCTYPE html>` can only start the template."));
            copy_text(i, e < hn ? e + 1 : hn, -1, "in the doctype", lit);
            i = e + 1;
            seen = 1;
            continue;
        }
        if (c == '<' && i + 1 < hn && hb[i + 1] == '/' && !(i + 2 < hn && is_hole(i + 2))) {
            long ns = i + 2, ne = ns;
            while (ne < hn && is_name_c((unsigned char)hb[ne])) ne++;
            long e = ne;
            while (e < hn && hb[e] != '>') e++;
            Str *name = lname(ns, ne);
            if (e < hn) end_tag(name, i, e + 1);
            else nest_err(i, hn, msg("The end tag `</$1` is not closed with `>`.", name, 0, 0));
            copy_text(i, e < hn ? e + 1 : hn, -1, "in an end tag", lit);
            i = e + 1;
            seen = 1;
            continue;
        }
        if (c == '<' && i + 1 < hn && ((hb[i + 1] | 32) >= 'a' && (hb[i + 1] | 32) <= 'z')) {
            i = start_tag(i, lit);
            seen = 1;
            continue;
        }
        if (is_brace_pair(i)) {
            sb_byte(htext, c);
            i += 2;
            seen = 1;
            continue;
        }
        if (is_hole(i)) {
            hole(i, HP_TEXT, 0, lit);
            i += 2;
            seen = 1;
            continue;
        }
        if (filled_hole_end(i)) {
            long e = filled_hole_end(i);
            herr_at("E0336", i, e, str_from_cstr(M_FILLED));
            hole(i, HP_TEXT, 0, lit);
            i = e;
            seen = 1;
            continue;
        }
        if (c == '{' || c == '}') herr_at("E0336", i, i + 1, str_from_cstr(M_BRACE));
        if (c == '<' && (is_hole(i + 1) || (i + 2 < hn && hb[i + 1] == '/' && is_hole(i + 2)))) {
            long h = is_hole(i + 1) ? i + 1 : i + 2; /* `<{}` or `</{}`: a value would choose the element */
            sb_bytes(htext, hb + i, h - i);
            hole(h, -1, "in a tag name", lit);
            i = h + 2;
            seen = 1;
            continue;
        }
        if (!is_space_c((unsigned char)c)) seen = 1;
        sb_byte(htext, c);
        i++;
    }
    if (hdepth && !hnest) {
        long a = hstack_at[hdepth - 1];
        long b = a + 1;
        while (b < hn && hb[b] != '>') b++;
        nest_err(a, b < hn ? b + 1 : hn, msg("`<$1>` is not closed.", hstack[hdepth - 1], 0, 0));
    }
}

static Type *html_call(Node *call);

Type *check_html_call(Node *call)
{
    Type *t = html_call(call);
    diag_const_node = 0;
    return t;
}

/* The kind of a value's type, or 0. */
static long kind_of(Type *t)
{
    if (!t || ty_bad(t)) return 0;
    if (t->kind == TY_STR) return HK_STR;
    if (t->kind == TY_INT) return HK_INT;
    if (t->kind == TY_FLOAT) return HK_FLOAT;
    if (t->kind == TY_BOOL) return HK_BOOL;
    if (ty_is_html(t)) return HK_HTML;
    if (t->kind == TY_LIST && ty_is_html(t->a.t)) return HK_HTML_LIST;
    return 0;
}

static Type *html_call(Node *call)
{
    Type *result = call->c.n->type;
    Vec *args = call->b.v;
    if (args->n == 0) {
        ck_err("E0302", call, str_from_cstr("`html` takes a template and the values for its holes, got no arguments."));
        return result;
    }
    long nv = args->n - 1;
    long *kinds = alloc((nv + 1) * 8);
    for (long i = 0; i < nv; i++) {
        Node *arg = vec_get(args, i + 1);
        Type *t = check_arg(call, i + 1, 0);
        kinds[i] = kind_of(t);
        if (!kinds[i] && t && !ty_bad(t)) err_one_of(arg, "`str`, `int`, `float`, `bool`, `Html` or `list[Html]`", t);
    }
    Node *lit = vec_get(args, 0);
    if (lit->kind != N_STR && !const_template(lit)) {
        check_arg(call, 0, 0);
        ck_err("E0335", lit, str_from_cstr("The template of `html` must be a string literal or a constant, so it can be checked: build a page by passing `html(...)` values into holes."));
        return result;
    }
    if (lit->flags & NF_CONST) diag_const_node = lit;
    check_arg(call, 0, t_str);
    hn = lit_decode(lit, &hb, &ho);
    if (hn < 0) return result;
    hfile = (int)lit->file;
    herr = 0;
    hnest = 0;
    hpieces = vec_new();
    htext = sb_new();
    hholes = 0;
    tokenize(lit);
    flush_text();
    if (hholes != nv) {
        Diag *d = diag_add("E0336", SEV_ERROR, hfile, lit->start, lit->end,
                           msg("The template has $1 hole(s) and $2 value(s) follow it.", int_to_str(hholes), int_to_str(nv), 0));
        (void)d;
        return result;
    }
    for (long i = 0; i < hpieces->n; i++) { /* each value against its hole's place */
        FmtPiece *p = vec_get(hpieces, i);
        if (p->text) continue;
        long k = kinds[p->arg];
        Node *arg = vec_get(args, p->arg + 1);
        if (!k) continue;
        if ((k == HK_HTML || k == HK_HTML_LIST) && p->spec != HP_TEXT) {
            ck_err("E0339", arg, str_from_cstr("Markup cannot go into an attribute: pass text (a `str`, `int`, `float` or `bool`)."));
            herr = 1;
        } else if (p->spec == HP_URL && k != HK_STR) {
            ck_err("E0339", arg, str_from_cstr("A URL attribute takes a `str`: build the URL as text first."));
            herr = 1;
        }
        p->spec = p->spec * 8 + k;
    }
    if (herr) return result;
    if (!hpieces->n) vec_push(hpieces, (void *)fmt_new_piece(str_new("", 0), -1, 0));
    call->e.p = hpieces;
    call->d.i = B_HTML;
    return result;
}

/* ---- run time ---- */

static void escape_into(SB *sb, Str *s, long from)
{
    for (long i = from; i < s->len; i++) {
        char c = s->data[i];
        switch (c) {
        case '&': sb_cstr(sb, "&amp;"); break;
        case '<': sb_cstr(sb, "&lt;"); break;
        case '>': sb_cstr(sb, "&gt;"); break;
        case '"': sb_cstr(sb, "&quot;"); break;
        case '\'': sb_cstr(sb, "&#39;"); break;
        default: sb_byte(sb, c);
        }
    }
}

/* Whether a URL is relative or of an allowed scheme (research R4). */
static int url_ok(Str *s)
{
    long i = 0;
    while (i < s->len && (unsigned char)s->data[i] <= ' ') i++; /* leading spaces and control bytes */
    long k = i;
    while (k < s->len && s->data[k] != ':' && s->data[k] != '/' && s->data[k] != '?' && s->data[k] != '#') k++;
    if (k >= s->len || s->data[k] != ':') return 1; /* no scheme: relative */
    long n = k - i;
    char low[8];
    if (n > 6) return 0;
    for (long j = 0; j < n; j++) low[j] = (char)lower_c((unsigned char)s->data[i + j]);
    low[n] = 0;
    return (n == 4 && mem_eq(low, "http", 4)) || (n == 5 && mem_eq(low, "https", 5)) || (n == 6 && mem_eq(low, "mailto", 6));
}

/* One hole's value as markup: spec = place * 8 + kind. */
Str *html_piece(long value, long spec)
{
    long place = spec >> 3, kind = spec & 7;
    if (kind == HK_HTML) return *(Str **)((char *)value + R_FIELDS);
    if (kind == HK_HTML_LIST) {
        List *xs = (List *)value;
        GC_ROOT(&xs);
        SB *sb = sb_new();
        GC_ROOT(&sb);
        for (long i = 0; i < xs->len; i++) sb_str(sb, *(Str **)((char *)xs->data[i].p + R_FIELDS));
        GC_UNROOT(2);
        return sb_finish(sb);
    }
    Str *s;
    if (kind == HK_STR)
        s = (Str *)value;
    else
        return fmt_piece(value, kind == HK_INT ? TY_INT : kind == HK_FLOAT ? TY_FLOAT : TY_BOOL, 0);
    if (place == HP_URL && !url_ok(s)) return str_from_cstr("#");
    GC_ROOT(&s);
    SB *sb = sb_new();
    escape_into(sb, s, 0);
    GC_UNROOT(1);
    return sb_finish(sb);
}
