/* Checker, spec 024: `format(template, args...)`. The template must be a string literal; it is
 * parsed from its source text, so every diagnostic points at the placeholder inside it (research
 * R1). Placeholders are checked against their arguments' types, habits from other languages get
 * fixes (`%d`, `{0}`, `{name}`, `{:.2}`, `{:d}`, `{:0>5}`), and the parsed pieces are stored on
 * the call (`call->e.p`, a Vec of FmtPiece) for the compiler (cformat). Codes E0315-E0318:
 * specs/024-string-formatting/contracts/diagnostics.md. */
#include "vo.h"

enum { PH_OK, PH_INDEX, PH_NAME, PH_BAD };

typedef struct {
    long ds, de; /* decoded range, `{` to `}` inclusive */
    long state;  /* PH_OK, PH_INDEX, PH_NAME, PH_BAD */
    long index;  /* PH_INDEX: the number */
    long fe;     /* decoded end of the field part (the index or name), after `{` */
    long spec;   /* PH_OK: packed (FS_* in vo.h) */
    int bad;     /* E0317 reported for it */
} Ph;

typedef struct {
    long ds, de; /* decoded range replaced */
    Str *text;   /* by this source text */
} Edit;

static char *fb;   /* the template, decoded */
static long *fo;   /* the source offset of each decoded byte; fo[fn] is the closing quote's */
static long fn;
static char *fsrc; /* the file's text */
static int ffile;
static Node *fcall;
static int ferr; /* errors reported for this call */

#define M_OPTIONS "Unsupported option `$1`: the options are `<` `>` `^` `+` `0` width `,` `.Nf` `x` `b`, in that order."

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int is_name_start(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_align(int c)
{
    return c == '<' || c == '>' || c == '^';
}

/* The source text of decoded bytes [a, b). */
static Str *dtext(long a, long b)
{
    return str_new(fsrc + fo[a], fo[b] - fo[a]);
}

/* A template diagnostic over decoded bytes [a, b). */
static Diag *ferr_at(char *code, long a, long b, Str *message)
{
    ferr = 1;
    return diag_add(code, SEV_ERROR, ffile, fo[a], fo[b], message);
}

/* `name(.name)*` over decoded [a, b). */
static int is_path(long a, long b)
{
    if (a >= b) return 0;
    int start = 1;
    for (long i = a; i < b; i++) {
        int c = (unsigned char)fb[i];
        if (start) {
            if (!is_name_start(c)) return 0;
            start = 0;
        } else if (c == '.') {
            start = 1;
        } else if (!is_name_start(c) && !is_digit(c)) {
            return 0;
        }
    }
    return !start;
}

/* The template literal rewritten: its source with each edit's decoded range replaced (edits in
 * order, not overlapping); the quotes included. */
static Str *rewrite(Edit *e, long ne)
{
    SB *sb = sb_new();
    sb_byte(sb, '"');
    long k = 0;
    for (long i = 0; i < fn;) {
        if (k < ne && e[k].ds == i) {
            sb_str(sb, e[k].text);
            i = e[k].de;
            k++;
            continue;
        }
        sb_bytes(sb, fsrc + fo[i], fo[i + 1] - fo[i]);
        i++;
    }
    sb_byte(sb, '"');
    return sb_finish(sb);
}

/* Spec 025: decodes the source between the quotes of string literal lit, with the source offset of
 * every decoded byte (offs[n] is the closing quote's); returns n, or -1 when lit is not a plain
 * one-line literal. Shared by format templates and regex patterns. */
long lit_decode(Node *lit, char **buf, long **offs)
{
    if (lit->flags & NF_CONST) { /* spec 031: a constant's value; every offset is the use (diag_const_node) */
        if (diag_const_node != lit) return -1;
        Str *v = lit->a.s;
        char *b = alloc(v->len + 1);
        long *o = alloc((v->len + 1) * 8);
        mem_copy(b, v->data, v->len);
        for (long i = 0; i < v->len; i++) o[i] = lit->start;
        o[v->len] = lit->end;
        *buf = b;
        *offs = o;
        return v->len;
    }
    char *src = file_get((int)lit->file)->text;
    long s = lit->start, e = lit->end, n = 0;
    if (e - s < 2 || src[s] != '"' || src[e - 1] != '"') return -1;
    char *b = alloc(e - s + 1);
    long *o = alloc((e - s + 1) * 8);
    for (long i = s + 1; i < e - 1;) {
        o[n] = i;
        int c = (unsigned char)src[i];
        if (c == '\\' && i + 1 < e - 1) {
            int k = (unsigned char)src[i + 1];
            b[n++] = (char)(k == 'n' ? '\n' : k == 't' ? '\t' : k);
            i += 2;
        } else {
            b[n++] = (char)c;
            i++;
        }
    }
    o[n] = e - 1;
    *buf = b;
    *offs = o;
    return n;
}

/* Decodes the template literal into fb, fo, fn; 0 when it is not a plain one-line literal. */
static int decode(Node *lit)
{
    fn = lit_decode(lit, &fb, &fo);
    return fn >= 0;
}

/* Whether the template has a `{}` or `{:...}` placeholder (not counting `{{`). */
static int has_placeholder(void)
{
    for (long i = 0; i + 1 < fn; i++) {
        if (fb[i] == '{' && fb[i + 1] == '{') {
            i++;
            continue;
        }
        if (fb[i] == '{' && (fb[i + 1] == '}' || fb[i + 1] == ':')) return 1;
    }
    return 0;
}

/* E0317 on placeholder ph. */
static Diag *bad_ph(Ph *ph, Str *message)
{
    ph->bad = 1;
    return ferr_at("E0317", ph->ds, ph->de, message);
}

/* The spec over decoded [p, q) (after the colon) of placeholder ph, packed into ph->spec; t is the
 * argument's type (0 when unknown), for the fixes. Returns 0 after an E0317. */
static int parse_spec(Ph *ph, long p, long q, Type *t)
{
    long spec = 0, s0 = p;
    int numeric = t && (t->kind == TY_INT || t->kind == TY_FLOAT);
    if (q - p >= 2 && is_align(fb[p + 1]) && !is_align(fb[p])) { /* a fill character */
        Diag *d = bad_ph(ph, str_from_cstr("Fill characters are not supported: use `0` for zeros or spaces by alignment."));
        int digits = q > p + 2;
        for (long i = p + 2; i < q; i++) digits &= is_digit(fb[i]);
        if (fb[p] == '0' && fb[p + 1] == '>' && numeric && digits)
            diag_add_fix(d, ffile, fo[p], fo[p + 2], str_from_cstr("0"));
        return 0;
    }
    if (p < q && is_align(fb[p])) spec |= fb[p++] == '<' ? 1 : fb[p - 1] == '>' ? 2 : 3;
    if (p < q && fb[p] == '+') {
        spec |= FS_PLUS;
        p++;
    }
    if (p < q && fb[p] == '0') {
        spec |= FS_ZERO;
        p++;
    }
    long w = 0, pr = 0;
    while (p < q && is_digit(fb[p])) {
        if (w <= 100000) w = w * 10 + (fb[p] - '0');
        p++;
    }
    if (p < q && fb[p] == ',') {
        spec |= FS_GROUP;
        p++;
    }
    if (p < q && fb[p] == '.') {
        long d0 = ++p;
        while (p < q && is_digit(fb[p])) {
            if (pr <= 100000) pr = pr * 10 + (fb[p] - '0');
            p++;
        }
        if (p == d0) {
            bad_ph(ph, msg(M_OPTIONS, str_from_cstr("."), 0, 0));
            return 0;
        }
        if (p == q) { /* Rust's (and Python's significant-digit) `.2`: decimals need `f` here */
            Str *n = dtext(d0, p);
            Diag *d = bad_ph(ph, msg("Precision needs `f`: `{:.$1f}` is $1 decimals.", n, 0, 0));
            diag_add_fix(d, ffile, fo[p], fo[p], str_from_cstr("f"));
            return 0;
        }
        if (fb[p] != 'f') {
            bad_ph(ph, msg(M_OPTIONS, dtext(p, p + 1), 0, 0));
            return 0;
        }
        p++;
        spec |= FS_PREC_ON;
    }
    if (p < q && (fb[p] == 'x' || fb[p] == 'b')) {
        spec |= (fb[p] == 'x' ? 1L : 2L) << 23;
        p++;
    } else if (p == q - 1 && (fb[p] == 'd' || fb[p] == 's')) { /* C and Python type letters */
        Diag *d = bad_ph(ph, msg("`$1` is not needed: the argument's type decides how it prints. Write `{}`.",
                                 dtext(p, q), 0, 0));
        long from = p == s0 ? s0 - 1 : p; /* `{:d}` loses its colon too */
        diag_add_fix(d, ffile, fo[from], fo[q], str_from_cstr(""));
        return 0;
    }
    if (p < q) {
        bad_ph(ph, msg(M_OPTIONS, dtext(p, p + 1), 0, 0));
        return 0;
    }
    if (w > 1000 || pr > 100) {
        bad_ph(ph, str_from_cstr("Width is at most 1000 and precision at most 100."));
        return 0;
    }
    char *with = 0, *without = 0;
    if ((spec & FS_ZERO) && (spec & 3)) {
        bad_ph(ph, str_from_cstr("`0` pads numbers by itself: drop the alignment."));
        return 0;
    }
    if ((spec & FS_ZERO) && (spec & FS_GROUP)) {
        bad_ph(ph, str_from_cstr("`0` pads numbers by itself: drop the `,`."));
        return 0;
    }
    if (FS_RADIX(spec) && (spec & FS_GROUP)) with = "`,`";
    if (FS_RADIX(spec) && (spec & FS_PREC_ON)) with = "a precision";
    if (with) {
        without = FS_RADIX(spec) == 1 ? "`x`" : "`b`";
        bad_ph(ph, msg("$1 cannot be combined with $2.", str_from_cstr(without), str_from_cstr(with), 0));
        return 0;
    }
    ph->spec = spec | w << 4 | pr << 16;
    return 1;
}

/* E0318 when an option of ph does not apply to the argument's type t (int, float, bool or str). */
static void check_options(Ph *ph, Node *arg, Type *t)
{
    long s = ph->spec, k = t->kind;
    char *opt = 0, *applies = 0;
    if (k == TY_STR || k == TY_BOOL) {
        applies = "numbers";
        opt = (s & FS_PLUS) ? "`+`" : (s & FS_ZERO) ? "`0`" : (s & FS_GROUP) ? "`,`" : 0;
    }
    if (!opt && k != TY_FLOAT && (s & FS_PREC_ON)) applies = "`float` values", opt = "precision";
    if (!opt && k != TY_INT && FS_RADIX(s)) applies = "`int` values", opt = FS_RADIX(s) == 1 ? "`x`" : "`b`";
    if (!opt) return;
    Str *o = str_from_cstr(opt);
    if (opt[0] == 'p') { /* `.Nf` as written */
        SB *sb = sb_new();
        sb_cstr(sb, "`.");
        sb_str(sb, int_to_str(FS_PREC(s)));
        sb_cstr(sb, "f`");
        o = sb_finish(sb);
    }
    Diag *d = ferr_at("E0318", ph->ds, ph->de, msg("$1 applies to $2, but the argument is `$3`.", o, str_from_cstr(applies), ty_name(t)));
    if (k == TY_INT && (s & FS_PREC_ON)) {
        SB *sb = sb_new();
        sb_cstr(sb, "float(");
        sb_str(sb, src_text(arg));
        sb_byte(sb, ')');
        diag_add_fix(d, (int)arg->file, arg->start, arg->end, sb_finish(sb));
    }
}

/* Spec 024 (FR-009): the template translated from C `%` verbs to placeholders, or 0 when any verb
 * does not translate or their number is not nargs. types[i] is argument i's type. */
static Str *translate_percent(Type **types, long nargs)
{
    Edit *e = alloc((fn + 1) * sizeof(Edit));
    long ne = 0, k = 0;
    for (long i = 0; i < fn;) {
        char c = fb[i];
        if (c == '{' || c == '}') { /* a literal brace becomes a doubled one */
            e[ne].ds = i;
            e[ne].de = i + 1;
            e[ne++].text = str_from_cstr(c == '{' ? "{{" : "}}");
            i++;
            continue;
        }
        if (c != '%') {
            i++;
            continue;
        }
        long v0 = i++;
        if (i < fn && fb[i] == '%') {
            e[ne].ds = v0;
            e[ne].de = ++i;
            e[ne++].text = str_from_cstr("%");
            continue;
        }
        int minus = 0, plus = 0, zero = 0;
        for (; i < fn && (fb[i] == '-' || fb[i] == '+' || fb[i] == '0'); i++) {
            minus |= fb[i] == '-';
            plus |= fb[i] == '+';
            zero |= fb[i] == '0';
        }
        long w0 = i;
        while (i < fn && is_digit(fb[i])) i++;
        long w1 = i, p0 = -1, p1 = -1;
        if (i < fn && fb[i] == '.') {
            p0 = ++i;
            while (i < fn && is_digit(fb[i])) i++;
            p1 = i;
        }
        if (i >= fn || k >= nargs) return 0;
        char verb = fb[i++];
        Type *t = types[k++];
        if (!t) return 0;
        long kind = t->kind;
        int num = kind == TY_INT || kind == TY_FLOAT, text = kind == TY_STR || kind == TY_BOOL;
        int ok = 0;
        switch (verb) {
        case 'd': case 'i': ok = kind == TY_INT; break;
        case 's': case 'v': ok = num || text; break;
        case 't': ok = kind == TY_BOOL; break;
        case 'f': ok = kind == TY_FLOAT; break;
        case 'x': case 'b': ok = kind == TY_INT; break;
        }
        if (!ok || (plus && !num) || (zero && !num) || (p0 >= 0 && verb != 'f')) return 0;
        SB *sb = sb_new();
        if (minus && num) sb_byte(sb, '<');
        if (!minus && w1 > w0 && text) sb_byte(sb, '>');
        if (plus) sb_byte(sb, '+');
        if (zero && !minus) sb_byte(sb, '0');
        sb_bytes(sb, fb + w0, w1 - w0);
        if (verb == 'f') {
            sb_byte(sb, '.');
            if (p0 >= 0 && p1 > p0)
                sb_bytes(sb, fb + p0, p1 - p0);
            else
                sb_cstr(sb, p0 >= 0 ? "0" : "6"); /* C: `%.f` is 0 decimals, `%f` is 6 */
            sb_byte(sb, 'f');
        }
        if (verb == 'x' || verb == 'b') sb_byte(sb, verb);
        Str *spec = sb_finish(sb);
        SB *ph = sb_new();
        sb_byte(ph, '{');
        if (spec->len) {
            sb_byte(ph, ':');
            sb_str(ph, spec);
        }
        sb_byte(ph, '}');
        e[ne].ds = v0;
        e[ne].de = i;
        e[ne++].text = sb_finish(ph);
    }
    if (k != nargs) return 0;
    return rewrite(e, ne);
}

/* E0317 fixes for indexed placeholders all in order, or named ones with no arguments. */
static void field_fixes(Ph *ph, long nph, long nargs, Node *call)
{
    long indexed = 0, named = 0;
    for (long i = 0; i < nph; i++) {
        indexed += ph[i].state == PH_INDEX && ph[i].index == indexed;
        named += ph[i].state == PH_NAME;
    }
    int by_index = indexed == nph && nph == nargs, by_name = named == nph && nargs == 0;
    if (!nph || (!by_index && !by_name)) return;
    Edit *e = alloc(nph * sizeof(Edit));
    for (long i = 0; i < nph; i++) { /* drop the field part: `{0:.2f}` → `{:.2f}` */
        e[i].ds = ph[i].ds + 1;
        e[i].de = ph[i].fe;
        e[i].text = str_from_cstr("");
    }
    Str *tmpl = rewrite(e, nph);
    Node *lit = vec_get(call->b.v, 0);
    long start = lit->start, end = lit->end;
    if (by_name) { /* format("...{}...", a, b.c): the whole call */
        SB *sb = sb_new();
        sb_str(sb, src_text(call->a.n));
        sb_byte(sb, '(');
        sb_str(sb, tmpl);
        for (long i = 0; i < nph; i++) {
            sb_cstr(sb, ", ");
            sb_str(sb, dtext(ph[i].ds + 1, ph[i].fe));
        }
        sb_byte(sb, ')');
        tmpl = sb_finish(sb);
        start = call->start;
        end = call->end;
    }
    for (long i = 0; i < diag_n; i++) { /* the fix on each of this template's field diagnostics */
        Diag *d = diag_arr[i];
        if (d->code[3] == '1' && d->code[4] == '7' && d->file == ffile && d->start >= fo[0] - 1 && d->end <= fo[fn] + 1)
            diag_add_fix(d, ffile, start, end, tmpl);
    }
}

static Type *format_call(Node *call);

/* spec 031: diagnostics inside a template that came from a constant are placed on its use. */
Type *check_format_call(Node *call)
{
    Type *t = format_call(call);
    diag_const_node = 0;
    return t;
}

static Type *format_call(Node *call)
{
    Vec *args = call->b.v;
    if (args->n == 0) {
        ck_err("E0302", call, str_from_cstr("`format` takes a template and the values for its placeholders, got no arguments."));
        return t_str;
    }
    long nargs = args->n - 1;
    Type **types = alloc((nargs + 1) * 8);
    for (long i = 0; i < nargs; i++) { /* the values: int, float, bool or str */
        Node *arg = vec_get(args, i + 1);
        Type *t = check_arg(call, i + 1, 0);
        long k = ty_bad(t) ? -1 : t->kind;
        if (k == TY_INT || k == TY_FLOAT || k == TY_BOOL || k == TY_STR)
            types[i] = t;
        else if (k >= 0)
            err_one_of(arg, "`int`, `float`, `bool` or `str`", t);
    }
    Node *lit = vec_get(args, 0);
    fcall = call;
    ffile = (int)lit->file;
    fsrc = file_get(ffile)->text;
    ferr = 0;
    if (lit->kind != N_STR && !const_template(lit)) {
        check_arg(call, 0, 0);
        ck_err("E0315", lit, str_from_cstr("The template of `format` must be a string literal or a constant, so its placeholders can be checked."));
        return t_str;
    }
    if (lit->flags & NF_CONST) diag_const_node = lit;
    check_arg(call, 0, t_str);
    if (!decode(lit)) return t_str;
    if (nargs && !has_placeholder()) { /* the C habit (`%d`, even inside braces: "{%d}") */
        Str *fixed = translate_percent(types, nargs);
        if (fixed) {
            Diag *d = ck_err("E0316", lit, msg("The template has $1 placeholder(s) and $2 argument(s) follow it.",
                                              int_to_str(0), int_to_str(nargs), 0));
            diag_add_fix(d, ffile, lit->start, lit->end, fixed);
            return t_str;
        }
    }
    /* the pieces: literal text and placeholders */
    Ph *ph = alloc((fn / 2 + 1) * sizeof(Ph));
    long nph = 0;
    Vec *pieces = vec_new();
    SB *text = sb_new();
    for (long i = 0; i < fn;) {
        char c = fb[i];
        if ((c == '{' || c == '}') && i + 1 < fn && fb[i + 1] == c) {
            sb_byte(text, c);
            i += 2;
            continue;
        }
        if (c == '}') {
            ferr_at("E0317", i, i + 1, str_from_cstr("Unmatched `}` in the template: write `{{` or `}}` for a brace."));
            i++;
            continue;
        }
        if (c != '{') {
            sb_byte(text, c);
            i++;
            continue;
        }
        long j = i + 1;
        while (j < fn && fb[j] != '}' && fb[j] != '{') j++;
        if (j >= fn || fb[j] != '}') {
            ferr_at("E0317", i, i + 1, str_from_cstr("Unmatched `{` in the template: write `{{` or `}}` for a brace."));
            i++;
            continue;
        }
        Ph *p = &ph[nph];
        p->ds = i;
        p->de = j + 1;
        p->bad = 0;
        p->spec = 0;
        long colon = i + 1;
        while (colon < j && fb[colon] != ':') colon++;
        p->fe = colon;
        p->state = PH_OK;
        if (colon > i + 1) { /* a field: `{0}`, `{name}`, ... */
            int digits = 1;
            p->index = 0;
            for (long k = i + 1; k < colon; k++) {
                digits &= is_digit(fb[k]);
                if (digits && p->index < 100000) p->index = p->index * 10 + (fb[k] - '0');
            }
            if (digits) {
                p->state = PH_INDEX;
                bad_ph(p, str_from_cstr("Placeholders take the arguments in order: write `{}` (no index)."));
            } else if (is_path(i + 1, colon)) {
                p->state = PH_NAME;
                bad_ph(p, msg("Placeholders take the arguments in order: write `{}` and pass `$1` as an argument.",
                              dtext(i + 1, colon), 0, 0));
            } else {
                p->state = PH_BAD;
                bad_ph(p, str_from_cstr("Placeholders take the arguments in order: write `{}` and pass the value as an argument."));
            }
        } else if (colon < j) {
            parse_spec(p, colon + 1, j, nph < nargs ? types[nph] : 0);
        }
        Str *t = sb_finish(text);
        if (t->len) vec_push(pieces, (void *)fmt_new_piece(t, -1, 0));
        text = sb_new();
        vec_push(pieces, (void *)fmt_new_piece(0, nph, 0)); /* its spec is set below */
        nph++;
        i = j + 1;
    }
    Str *t = sb_finish(text);
    if (t->len) vec_push(pieces, (void *)fmt_new_piece(t, -1, 0));
    if (ferr) {
        field_fixes(ph, nph, nargs, call);
        return t_str;
    }
    if (nph != nargs) {
        Diag *d = ferr_at("E0316", 0, fn, msg("The template has $1 placeholder(s) and $2 argument(s) follow it.",
                                                  int_to_str(nph), int_to_str(nargs), 0));
        d->start = lit->start; /* the whole literal, quotes included */
        d->end = lit->end;
        Str *fixed = nph == 0 && nargs > 0 ? translate_percent(types, nargs) : 0;
        if (fixed) diag_add_fix(d, ffile, lit->start, lit->end, fixed);
        return t_str;
    }
    for (long i = 0; i < nph; i++)
        if (types[i]) check_options(&ph[i], vec_get(args, i + 1), types[i]);
    if (ferr) return t_str;
    if (!pieces->n) vec_push(pieces, (void *)fmt_new_piece(str_new("", 0), -1, 0)); /* format(""): one constant */
    for (long i = 0; i < pieces->n; i++) { /* the specs, now that they are checked */
        FmtPiece *pc = vec_get(pieces, i);
        if (!pc->text) pc->spec = ph[pc->arg].spec;
    }
    call->e.p = pieces;
    return t_str;
}

FmtPiece *fmt_new_piece(Str *text, long arg, long spec)
{
    FmtPiece *p = alloc(sizeof(FmtPiece));
    p->text = text;
    p->arg = arg;
    p->spec = spec;
    return p;
}
