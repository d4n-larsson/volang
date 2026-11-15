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
    "ensures", "export", 0,
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

/* Letters right after a number (1e3, 10px): E0105 over the whole run; the letters are skipped, so
 * lexing goes on after them. */
static void lx_number_suffix(long start, long end)
{
    if (end >= lx_len || !is_word_char((unsigned char)lx_text[end])) return;
    while (end < lx_len && is_word_char((unsigned char)lx_text[end])) end++;
    lx_err("E0105", start, end,
           str_from_cstr("Number literals have no exponent or suffix: write the number in full, like 1000.0."));
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
                if (e == 'n' || e == 't' || e == '\\' || e == '"') {
                    pos += 2;
                    continue;
                }
                long k = utf8_seq_len(text + pos + 1, len - (pos + 1)); /* unknown escape: report \x */
                if (k == 0) k = 1;
                long esc_end = pos + 1 + k;
                lx_err("E0103", pos, esc_end,
                       msg("Unknown escape sequence `$1`. Use \\n, \\t, \\\\ or \\\".", lx_slice(pos, esc_end), 0, 0));
                pos = esc_end;
            }
            continue;
        }
        if (c >= '0' && c <= '9') { /* number */
            while (pos < len && text[pos] >= '0' && text[pos] <= '9') pos++;
            if (pos + 1 < len && text[pos] == '.' && text[pos + 1] >= '0' && text[pos + 1] <= '9') {
                pos++;
                while (pos < len && text[pos] >= '0' && text[pos] <= '9') pos++;
                lx_number_suffix(start, pos);
                lx_push(T_FLOAT, start, pos);
                long count = 0, through = 0; /* significant digits: first through last non-zero */
                int started = 0;
                for (long i = start; i < pos; i++) {
                    char d = text[i];
                    if (d == '.') continue;
                    if (!started) {
                        if (d == '0') continue;
                        started = 1;
                    }
                    count++;
                    if (d != '0') through = count;
                }
                if (through > 15)
                    lx_err("E0105", start, pos, str_from_cstr("Float literal has more than 15 significant digits."));
            } else {
                lx_number_suffix(start, pos);
                lx_push(T_INT, start, pos);
                long value;
                if (dec_to_int(text + start, pos - start, &value))
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
            break;
        case '>':
            kind = T_GT;
            if (c2 == '=') kind = T_GE, tlen = 2;
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
