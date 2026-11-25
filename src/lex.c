/* Lexer: source bytes -> 16-byte tokens (contracts/grammar.md, lexical rules; ported from
 * src/lex.s, spec 008). */
#include "vo.h"

static char *lx_text;
static long lx_len;
static long lx_file;
static Token *lx_toks;
static long lx_n;
static long lx_nl;      /* a newline was seen since the last token */
static long lx_code;    /* the current line has a token */
static long lx_lstart;  /* offset where the current line starts */
static long lx_skip_to; /* set by lx_number_suffix: resume lexing here */

/* Keywords, in token-kind order from T_KW_FIRST. */
static const char *const kw_table[] = {
    "fn",     "record", "variant", "test",  "import", "let",   "var",   "set",      "if",
    "else",   "while",  "for",     "in",    "match",  "case",  "return", "break",   "continue",
    "try",    "new",    "and",     "or",    "not",    "true",  "false", "int",      "float",
    "bool",   "str",    "list",    "map",   "result", "void",  "uses",  "prop",     "requires",
    "ensures", "export", "bytes", 0,
};

static void lx_push(int kind, long start, long end)
{
    Token *t = &lx_toks[lx_n++];
    t->kind = (uint16_t)kind;
    t->flags = (uint16_t)lx_nl;
    lx_nl = 0;
    t->start = (uint32_t)start;
    t->end = (uint32_t)end;
    lx_code = 1;
}

/* Adds an error diagnostic for the current file. */
static Diag *lx_err(char *code, long start, long end, Str *message)
{
    return diag_add(code, SEV_ERROR, (int)lx_file, start, end, message);
}

static Str *lx_slice(long start, long end)
{
    return str_new(lx_text + start, end - start);
}

int is_hex_digit(int c)
{
    return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'f');
}

int is_word_char(int c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* The keyword token kind for the word, or T_IDENT. */
int keyword_kind(char *p, long n)
{
    for (int i = 0; kw_table[i]; i++)
        if (cstr_len((char *)kw_table[i]) == n && mem_eq(p, (char *)kw_table[i], n)) return T_KW_FIRST + i;
    return T_IDENT;
}

/* Spec 042: the escape `\u...` at pos (its backslash): the offset after it. A well-formed one is
 * `\u{H}` to `\u{HHHHHH}` naming a Unicode scalar value that has no short escape; anything else is
 * E0103, with a fix where exactly one correction exists. */
static long lx_unicode_escape(long pos)
{
    char *t = lx_text;
    long len = lx_len, i = pos + 2, v = 0, digits = 0;
    if (i >= len || t[i] != '{') { /* å: the form of other languages */
        long k = i;
        while (k < len && k - i < 6 && ((t[k] >= '0' && t[k] <= '9') || (t[k] >= 'a' && t[k] <= 'f') || (t[k] >= 'A' && t[k] <= 'F'))) k++;
        Diag *d = lx_err("E0103", pos, k, str_from_cstr("A Unicode escape is written with braces: `\\u{e5}`."));
        if (k - i == 4) { /* \uHHHH: the same digits in braces */
            SB *fix = sb_new();
            sb_cstr(fix, "\\u{");
            sb_str(fix, lx_slice(i, k));
            sb_byte(fix, '}');
            diag_add_fix(d, (int)lx_file, pos, k, sb_finish(fix));
        }
        return k;
    }
    long k = i + 1;
    for (; k < len; k++, digits++) {
        int h = (unsigned char)t[k];
        int dgt = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 87 : h >= 'A' && h <= 'F' ? h - 55 : -1;
        if (dgt < 0) break;
        if (digits < 7) v = v * 16 + dgt;
    }
    int closed = k < len && t[k] == '}';
    long end = closed ? k + 1 : k;
    char *why = !digits                                  ? "A Unicode escape needs 1 to 6 hexadecimal digits in its braces: `\\u{e5}`."
                : digits > 6                             ? "A Unicode escape has at most 6 hexadecimal digits."
                : v > 0x10ffff                           ? "A Unicode escape names a character up to 10FFFF."
                : v >= 0xd800 && v <= 0xdfff             ? "A Unicode escape cannot name a surrogate (D800 to DFFF): they are not characters."
                : !closed && k < len && t[k] != '"' && t[k] != '\n' && t[k] != ' ' ? "A Unicode escape holds hexadecimal digits only: `\\u{e5}`."
                                                         : 0;
    if (why) {
        lx_err("E0103", pos, end, str_from_cstr(why));
        return end;
    }
    if (!closed) {
        Diag *d = lx_err("E0103", pos, end, str_from_cstr("This Unicode escape is not closed: add `}`."));
        diag_add_fix(d, (int)lx_file, end, end, str_from_cstr("}"));
        return end;
    }
    char *shorter = v == 9 ? "\\t" : v == 10 ? "\\n" : v == 13 ? "\\r" : v == 0x22 ? "\\\"" : v == 0x5c ? "\\\\" : 0;
    if (shorter) { /* one escape per character */
        Diag *d = lx_err("E0103", pos, end, msg("This character has a short escape: write `$1`.", str_from_cstr(shorter), 0, 0));
        diag_add_fix(d, (int)lx_file, pos, end, str_from_cstr(shorter));
    }
    return end;
}

/* Letters right after a number (10px, 1ex): E0105 over the whole run; the letters are skipped, so
 * lexing goes on after them. */
static void lx_number_suffix(long start, long end)
{
    if (end >= lx_len || !is_word_char((unsigned char)lx_text[end])) return;
    while (end < lx_len && is_word_char((unsigned char)lx_text[end])) end++;
    lx_err("E0105", start, end,
           str_from_cstr("Number literals have no suffix: write the number alone, like 1000 or 1.5e3."));
    lx_skip_to = end; /* tell the caller where lexing resumes */
}

/* A habit from other languages (&&, ||, !): E0101 with a fix replacing it by the vo keyword
 * (spaced so it stays a separate word); lexing goes on as if the keyword had been written.
 * Returns the position after the operator. */
static long lx_carry(long pos, long len, char *word, int kind)
{
    long end = pos + len;
    Str *op = lx_slice(pos, end);
    Diag *d = lx_err("E0101", pos, end, msg("vo writes `$1`, not `$2`.", str_from_cstr(word), op, 0));
    SB *sb = sb_new(); /* replacement, with spaces where a word would touch */
    if (pos != 0 && lx_text[pos - 1] != ' ') sb_byte(sb, ' ');
    sb_cstr(sb, word);
    if (end < lx_len && lx_text[end] != ' ') sb_byte(sb, ' ');
    diag_add_fix(d, (int)lx_file, pos, end, sb_finish(sb));
    lx_push(kind, pos, end);
    return end;
}

/* 'text' on one line: E0101 with a fix using double quotes, lexed as a string. Returns the
 * position after the closing quote, or 0 when there is none on the line. */
static long lx_single_quote(long pos)
{
    long end = pos + 1;
    for (;; end++) {
        if (end >= lx_len || lx_text[end] == '\n') return 0;
        if (lx_text[end] == '\'') break;
    }
    end++; /* after the closing quote */
    Diag *d = lx_err("E0101", pos, end, str_from_cstr("Strings use double quotes."));
    SB *sb = sb_new();
    sb_cstr(sb, "\"");
    sb_str(sb, lx_slice(pos + 1, end - 1));
    sb_cstr(sb, "\"");
    diag_add_fix(d, (int)lx_file, pos, end, sb_finish(sb));
    lx_push(T_STRING, pos, end);
    return end;
}

/* Spec 024: f"..." at pos (the `f`), the string closed at end. E0120 over it, with a fix to the
 * equivalent `format` call when every hole is `name(.name)*`, optionally followed by `:spec`. */
static void lx_fstring(long pos, long end)
{
    Diag *d = lx_err("E0120", pos, end,
                     str_from_cstr("vo has no f-strings: use `format` with `{}` placeholders and the values as arguments."));
    SB *tmpl = sb_new(), *args = sb_new(), *plain = sb_new(); /* plain: the text with no holes */
    for (long i = pos + 2; i < end - 1;) {
        char c = lx_text[i];
        if ((c == '{' || c == '}') && lx_text[i + 1] == c && i + 1 < end - 1) {
            sb_byte(tmpl, c);
            sb_byte(tmpl, c);
            sb_byte(plain, c);
            i += 2;
            continue;
        }
        if (c == '}') return; /* a lone brace: no fix */
        if (c != '{') {
            sb_byte(tmpl, c);
            sb_byte(plain, c);
            i++;
            continue;
        }
        long j = i + 1, colon = 0;
        while (j < end - 1 && lx_text[j] != '}') {
            if (lx_text[j] == ':' && !colon) colon = j;
            j++;
        }
        if (j >= end - 1) return;
        long e = colon ? colon : j; /* the hole's expression: i + 1 .. e */
        int start = 1;
        for (long k = i + 1; k < e; k++) {
            unsigned char h = (unsigned char)lx_text[k];
            if (start) {
                if (!is_word_char(h) || (h >= '0' && h <= '9')) return;
                start = 0;
            } else if (h == '.') {
                start = 1;
            } else if (!is_word_char(h)) {
                return;
            }
        }
        if (start) return; /* empty, or ending with `.` */
        sb_byte(tmpl, '{');
        if (colon) sb_str(tmpl, lx_slice(colon, j));
        sb_byte(tmpl, '}');
        sb_cstr(args, ", ");
        sb_str(args, lx_slice(i + 1, e));
        i = j + 1;
    }
    SB *fix = sb_new();
    Str *a = sb_finish(args);
    if (a->len) sb_cstr(fix, "format(");
    sb_byte(fix, '"');
    sb_str(fix, a->len ? sb_finish(tmpl) : sb_finish(plain));
    sb_byte(fix, '"');
    if (a->len) {
        sb_str(fix, a);
        sb_byte(fix, ')');
    }
    diag_add_fix(d, (int)lx_file, pos, end, sb_finish(fix));
}

/* Tokenizes a registered file; stores and returns the token array (ending with T_EOF). */
Token *lex(int file)
{
    static const char hexup[] = "0123456789ABCDEF";
    lx_file = file;
    FileRec *f = file_get(file);
    char *text = f->text;
    long len = f->len;
    lx_text = text;
    lx_len = len;
    lx_toks = alloc((len + 2) * 16);
    f->tokens = lx_toks;
    lx_n = 0;
    lx_nl = 1;
    lx_code = 0;
    lx_lstart = 0;
    long bad = utf8_validate(text, len); /* UTF-8 check first: report the first bad byte */
    if (bad >= 0) {
        unsigned b = (unsigned char)text[bad];
        char *hex = alloc(2);
        hex[0] = hexup[b >> 4];
        hex[1] = hexup[b & 15];
        lx_err("E0104", bad, bad + 1, msg("Invalid UTF-8 byte 0x$1.", str_new(hex, 2), 0, 0));
    }
    long pos = 0;
    while (pos < len) {
        int c = (unsigned char)text[pos];
        long start = pos;
        if (c == '\n') {
            pos++;
            lx_nl = 1;
            lx_code = 0;
            lx_lstart = pos;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            continue;
        }
        if (c == '#') { /* comment */
            while (pos < len && text[pos] != '\n') pos++;
            if (!lx_code) {
                lx_push(T_COMMENT, start, pos);
                lx_code = 0; /* a comment line does not make the line "code" */
                continue;
            }
            /* trailing comment: E0101, fix moves it to its own line */
            Diag *d = lx_err("E0101", start, pos, str_from_cstr("A comment must be on its own line."));
            SB *sb = sb_new();
            sb_byte(sb, '\n');
            for (long i = lx_lstart; text[i] == ' ' || text[i] == '\t'; i++) sb_byte(sb, text[i]);
            diag_add_fix(d, (int)lx_file, start, start, sb_finish(sb));
            continue;
        }
        if (c == 'f' && pos + 1 < len && text[pos + 1] == '"') { /* spec 024: f"...": E0120, then a plain string */
            long e = pos + 2;
            while (e < len && text[e] != '\n' && text[e] != '"') e += text[e] == '\\' && e + 1 < len && text[e + 1] != '\n' ? 2 : 1;
            if (e < len && text[e] == '"') lx_fstring(pos, e + 1);
            pos++;
            continue;
        }
        if (c == '`') { /* spec 042: a raw string, to the next backtick; line breaks are part of it */
            pos++;
            while (pos < len && text[pos] != '`') {
                if (text[pos] == '\n') lx_lstart = pos + 1;
                pos++;
            }
            if (pos < len) {
                pos++;
                lx_push(T_STRING, start, pos);
            } else {
                lx_push(T_STRING, start, pos);
                lx_err("E0102", start, start + 1,
                       str_from_cstr("This raw string is not closed: it runs from this backtick to the end of the file. Add a backtick "
                                     "where the text ends."));
            }
            lx_code = 1;
            continue;
        }
        if (c == '"') { /* string */
            pos++;
            for (;;) {
                if (pos >= len || text[pos] == '\n') { /* unterminated: E0102, fix inserts the quote */
                    lx_push(T_STRING, start, pos);
                    Diag *d = lx_err("E0102", start, pos,
                                     str_from_cstr("This string is not closed before the end of the line."));
                    diag_add_fix(d, (int)lx_file, pos, pos, str_from_cstr("\""));
                    break;
                }
                if (text[pos] == '"') {
                    pos++;
                    lx_push(T_STRING, start, pos);
                    break;
                }
                if (text[pos] != '\\') {
                    pos++;
                    continue;
                }
                if (pos + 1 >= len || text[pos + 1] == '\n') {
                    lx_push(T_STRING, start, pos);
                    Diag *d = lx_err("E0102", start, pos,
                                     str_from_cstr("This string is not closed before the end of the line."));
                    diag_add_fix(d, (int)lx_file, pos, pos, str_from_cstr("\""));
                    break;
                }
                char e = text[pos + 1];
                if (e == 'n' || e == 't' || e == '\\' || e == '"' || e == 'r') { /* spec 042: \r */
                    pos += 2;
                    continue;
                }
                if (e == 'u') { /* spec 042: \u{HEX} */
                    pos = lx_unicode_escape(pos);
                    continue;
                }
                long k = utf8_seq_len(text + pos + 1, len - (pos + 1)); /* unknown escape: report \x */
                if (k == 0) k = 1;
                long esc_end = pos + 1 + k;
                Diag *d = lx_err("E0103", pos, esc_end,
                                 msg("Unknown escape sequence `$1`. Use \\n, \\t, \\r, \\\\, \\\" or \\u{HEX}.", lx_slice(pos, esc_end), 0, 0));
                if (e == '0') /* spec 042: a NUL has one spelling */
                    diag_add_fix(d, (int)lx_file, pos, esc_end, str_from_cstr("\\u{0}"));
                else
                    diag_add_fix(d, (int)lx_file, pos, pos, str_from_cstr("\\")); /* spec 025: a literal backslash */
                pos = esc_end;
            }
            continue;
        }
        if (c == '0' && start + 1 < len && ((text[start + 1] | 32) == 'x' || (text[start + 1] | 32) == 'b')) {
            /* spec 043: 0x and 0b integers, a 64-bit pattern. (0x1e3 is hex: this rule comes first.) */
            int hex = (text[start + 1] | 32) == 'x';
            pos = start + 2;
            while (pos < len && (hex ? is_hex_digit(text[pos]) : text[pos] == '0' || text[pos] == '1')) pos++;
            long digits = pos - (start + 2);
            if (text[start + 1] < 'a') { /* 0X, 0B: one spelling */
                Diag *d = lx_err("E0105", start, pos, msg("Write the prefix in lower case: `$1`.", str_from_cstr(hex ? "0x" : "0b"), 0, 0));
                diag_add_fix(d, (int)lx_file, start + 1, start + 2, str_from_cstr(hex ? "x" : "b"));
            }
            if (!digits) {
                while (pos < len && is_word_char((unsigned char)text[pos])) pos++; /* 0xg: the letters go with it */
                lx_err("E0105", start, pos,
                       str_from_cstr(hex ? "A `0x` literal needs hex digits after it, like `0xFF`."
                                         : "A `0b` literal needs binary digits after it, like `0b1010`."));
            } else if (digits > (hex ? 16 : 64)) {
                lx_err("E0105", start, pos,
                       str_from_cstr(hex ? "A hex literal has at most 16 digits (64 bits)." : "A binary literal has at most 64 digits."));
            } else {
                lx_number_suffix(start, pos);
            }
            lx_push(T_INT, start, start + 2 + digits);
            if (lx_skip_to) {
                pos = lx_skip_to;
                lx_skip_to = 0;
            }
            continue;
        }
        if (c >= '0' && c <= '9') { /* number */
            while (pos < len && text[pos] >= '0' && text[pos] <= '9') pos++;
            if (c == '0' && pos - start > 1) { /* spec 056: 00012, 007.5: one spelling, without leading zeros */
                long z = start;
                while (z + 1 < pos && text[z] == '0') z++;
                Diag *d = lx_err("E0105", start, pos, str_from_cstr("Write the number without leading zeros."));
                diag_add_fix(d, (int)lx_file, start, z, str_from_cstr(""));
            }
            int is_float = 0;
            if (pos + 1 < len && text[pos] == '.' && text[pos + 1] >= '0' && text[pos + 1] <= '9') {
                is_float = 1;
                pos++;
                while (pos < len && text[pos] >= '0' && text[pos] <= '9') pos++;
            }
            long mant_end = pos, bad_exp = 0;
            if (pos < len && (text[pos] == 'e' || text[pos] == 'E')) { /* spec 042: an exponent makes a float */
                long k = pos + 1;
                if (k < len && (text[k] == '+' || text[k] == '-')) k++;
                if (k < len && text[k] >= '0' && text[k] <= '9') {
                    while (k < len && text[k] >= '0' && text[k] <= '9') k++;
                    is_float = 1;
                    if (text[pos] == 'E' || text[pos + 1] == '+') { /* spec 056: 1E9, 1e+9: one spelling, 1e9 */
                        SB *sb = sb_new();
                        sb_byte(sb, 'e');
                        long from = pos + 1 + (text[pos + 1] == '+');
                        sb_bytes(sb, text + from, k - from);
                        Str *exp = sb_finish(sb);
                        Diag *d = lx_err("E0105", start, k, msg("Write `$1$2`: the exponent is a lower-case `e`, without `+`.",
                                                                lx_slice(start, pos), exp, 0));
                        diag_add_fix(d, (int)lx_file, pos, k, exp);
                    }
                    pos = k;
                } else if (k >= len || !is_word_char((unsigned char)text[k])) { /* 1e, 1e+: an exponent without digits */
                    is_float = 1;
                    bad_exp = 1;
                    pos = k;
                }
            }
            if (is_float) {
                if (bad_exp)
                    lx_err("E0105", start, pos, str_from_cstr("An exponent needs digits: write `1e9` or `1.5e-3`."));
                else
                    lx_number_suffix(start, pos);
                lx_push(T_FLOAT, start, bad_exp ? mant_end : pos);
                long count = 0, through = 0; /* significant digits: first through last non-zero */
                int started = 0;
                for (long i = start; i < mant_end; i++) {
                    char d = text[i];
                    if (d == '.') continue;
                    if (!started) {
                        if (d == '0') continue;
                        started = 1;
                    }
                    count++;
                    if (d != '0') through = count;
                }
                if (through > 17)
                    lx_err("E0105", start, pos, str_from_cstr("Float literal has more than 17 significant digits."));
                else if (!bad_exp && ((unsigned long)text_to_float(text + start, pos - start) << 1) == (2047UL << 53))
                    lx_err("E0105", start, pos, str_from_cstr("Float literal is larger than the largest float (about 1.8e308)."));
                else if (!bad_exp && through > 0 && text_to_float(text + start, pos - start) == 0) /* spec 056 */
                    lx_err("E0105", start, pos, str_from_cstr("Float literal is smaller than the smallest float above "
                                                              "zero (about 4.9e-324): write `0.0`."));
            } else {
                lx_number_suffix(start, pos);
                lx_push(T_INT, start, pos);
                long value;
                if (int_literal_value(text + start, pos - start, &value))
                    lx_err("E0105", start, pos, str_from_cstr("Integer literal is larger than 9223372036854775807."));
            }
            if (lx_skip_to) { /* resume after a skipped suffix (1e3) */
                pos = lx_skip_to;
                lx_skip_to = 0;
            }
            continue;
        }
        if (is_word_char(c)) { /* word: identifier, type name, keyword or _ */
            while (pos < len && is_word_char((unsigned char)text[pos])) pos++;
            int kind;
            if (pos - start == 1 && c == '_')
                kind = T_UNDERSCORE;
            else if (c >= 'A' && c <= 'Z')
                kind = T_TYPE_IDENT;
            else
                kind = keyword_kind(text + start, pos - start);
            lx_push(kind, start, pos);
            continue;
        }
        /* punctuation */
        int c2 = pos + 1 < len ? (unsigned char)text[pos + 1] : 0; /* second byte (0 at end) */
        int kind = 0;
        long tlen = 1;
        switch (c) {
        case '(': kind = T_LPAREN; break;
        case ')': kind = T_RPAREN; break;
        case '{': kind = T_LBRACE; break;
        case '}': kind = T_RBRACE; break;
        case '[': kind = T_LBRACKET; break;
        case ']': kind = T_RBRACKET; break;
        case ',': kind = T_COMMA; break;
        case '@': kind = T_AT; break; /* spec 012 */
        case ';': kind = T_SEMI; break;
        case ':': kind = T_COLON; break;
        case '+': kind = T_PLUS; break;
        case '*': kind = T_STAR; break;
        case '/': kind = T_SLASH; break;
        case '%': kind = T_PERCENT; break;
        case '.':
            kind = T_DOT;
            if (c2 == '.') kind = T_DOTDOT, tlen = 2;
            break;
        case '-':
            kind = T_MINUS;
            if (c2 == '>') kind = T_ARROW, tlen = 2;
            break;
        case '=':
            kind = T_ASSIGN;
            if (c2 == '=') kind = T_EQ, tlen = 2;
            break;
        case '<':
            kind = T_LT;
            if (c2 == '=') kind = T_LE, tlen = 2;
            if (c2 == '<') kind = 0; /* spec 043: `<<` is reported as one habit, below */
            break;
        case '>':
            kind = T_GT;
            if (c2 == '=') kind = T_GE, tlen = 2;
            if (c2 == '>') kind = 0;
            break;
        case '!':
            if (c2 == '=') {
                kind = T_NE;
                tlen = 2;
                break;
            }
            pos = lx_carry(pos, 1, "not", T_NOT); /* `!x`: vo writes `not` */
            continue;
        case '&':
            if (c2 == '&') {
                pos = lx_carry(pos, 2, "and", T_AND); /* `&&`: vo writes `and` */
                continue;
            }
            break;
        case '|':
            if (c2 == '|') {
                pos = lx_carry(pos, 2, "or", T_OR); /* `||`: vo writes `or` */
                continue;
            }
            break;
        case '\'': { /* single quote: strings use double quotes */
            long after = lx_single_quote(pos);
            if (after) {
                pos = after;
                continue;
            }
            break;
        }
        case '?':
            lx_err("E0101", pos, pos + 1,
                   str_from_cstr("vo has no `?:` or `?` operator: use `if`/`else`, or `try` to pass an error on."));
            pos++;
            continue;
        default: break;
        }
        if (kind) {
            lx_push(kind, pos, pos + tlen);
            pos += tlen;
            continue;
        }
        if (c == '&' || c == '|' || c == '^' || c == '~' || c == '<' || c == '>') { /* spec 043: bit operators */
            long end = pos + (c == '<' || c == '>' ? 2 : 1);
            lx_err("E0101", pos, end,
                   msg("vo has no `$1` operator: for bits, use `std.bits` (`bit_and`, `shift_left`, ...).", lx_slice(pos, end), 0, 0));
            lx_push(c == '~' ? T_MINUS : T_PLUS, pos, end); /* one error: the parser reads on as if it were arithmetic */
            pos = end;
            continue;
        }
        /* anything else: E0101 with a removal fix */
        long k = utf8_seq_len(text + pos, len - pos);
        if (k == 0) k = 1;
        long end = pos + k;
        Diag *d = lx_err("E0101", pos, end, msg("Unexpected character `$1`.", lx_slice(pos, end), 0, 0));
        diag_add_fix(d, (int)lx_file, pos, end, str_new(0, 0));
        pos = end;
    }
    lx_push(T_EOF, len, len);
    return lx_toks;
}
