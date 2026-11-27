/* std.json's text side in C: `__json_parse(s)` and `__json_write(j, pretty)`. The parser used to be
 * vo code walking chars(s), at 40 bytes and several calls per character; this one reads the bytes
 * in place. Values, messages and positions are the ones std/json.vo produced (tests/json, the
 * corpus of tests/json_corpus.py).
 *
 * A failed parse returns 0 with the message in jp_err; values are never 0. The source string is
 * rooted by json_parse and the collector does not move objects, so jp_s stays valid. */
#include "vo.h"

enum { J_NULL, J_BOOL, J_INT, J_NUM, J_TEXT, J_ARRAY, J_OBJECT }; /* std.json's Json cases, in order */

#define JP_DEPTH 512

static char *jp_s;   /* the text */
static long jp_n;    /* its length */
static long jp_i;    /* the next byte */
static Str *jp_err;  /* the message of a failed parse */
static Node *jp_call;

static long jp_value(long depth);

static void sb_long(SB *sb, long v) /* v >= 0 */
{
    char buf[20];
    int n = 0;
    do buf[n++] = (char)('0' + v % 10);
    while (v /= 10);
    while (n) sb_byte(sb, buf[--n]);
}

/* Fails with "line L, column C: what" for the byte at `at` (columns count characters); with found,
 * the character there follows in backquotes. */
static long jp_fail_found(long at, char *what, int found)
{
    long line = 1, start = 0;
    for (long i = 0; i < at; i++)
        if (jp_s[i] == '\n') {
            line++;
            start = i + 1;
        }
    long col = 1;
    for (long i = start; i < at; i++)
        if (((unsigned char)jp_s[i] & 0xC0) != 0x80) col++;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "line ");
    sb_long(sb, line);
    sb_cstr(sb, ", column ");
    sb_long(sb, col);
    sb_cstr(sb, ": ");
    sb_cstr(sb, what);
    if (found) {
        long k = utf8_seq_len(jp_s + at, jp_n - at);
        sb_byte(sb, '`');
        sb_bytes(sb, jp_s + at, k ? k : 1);
        sb_byte(sb, '`');
    }
    jp_err = sb_finish(sb);
    GC_UNROOT(1);
    return 0;
}

static long jp_fail(long at, char *what)
{
    return jp_fail_found(at, what, 0);
}

static void jp_space(void)
{
    while (jp_i < jp_n && (jp_s[jp_i] == ' ' || jp_s[jp_i] == '\n' || jp_s[jp_i] == '\t' || jp_s[jp_i] == '\r')) jp_i++;
}

static int jp_digit(void)
{
    return jp_i < jp_n && jp_s[jp_i] >= '0' && jp_s[jp_i] <= '9';
}

static void sb_utf8(SB *sb, long cp)
{
    if (cp < 0x80) {
        sb_byte(sb, (int)cp);
    } else if (cp < 0x800) {
        sb_byte(sb, (int)(cp >> 6 | 0xC0));
        sb_byte(sb, (int)((cp & 0x3f) | 0x80));
    } else if (cp < 0x10000) {
        sb_byte(sb, (int)(cp >> 12 | 0xE0));
        sb_byte(sb, (int)((cp >> 6 & 0x3f) | 0x80));
        sb_byte(sb, (int)((cp & 0x3f) | 0x80));
    } else {
        sb_byte(sb, (int)(cp >> 18 | 0xF0));
        sb_byte(sb, (int)((cp >> 12 & 0x3f) | 0x80));
        sb_byte(sb, (int)((cp >> 6 & 0x3f) | 0x80));
        sb_byte(sb, (int)((cp & 0x3f) | 0x80));
    }
}

/* Four hex digits at jp_i: their value, or -1 (failed at the first that is not one). */
static long jp_hex4(void)
{
    long v = 0;
    for (int k = 0; k < 4; k++) {
        int c = jp_i < jp_n ? (unsigned char)jp_s[jp_i] : 0, d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else {
            jp_fail(jp_i, "expected four hex digits after \\u");
            return -1;
        }
        v = v * 16 + d;
        jp_i++;
    }
    return v;
}

/* The string whose opening quote is at jp_i, or 0. */
static Str *jp_string(void)
{
    long from = ++jp_i;
    while (jp_i < jp_n && jp_s[jp_i] != '"' && jp_s[jp_i] != '\\' && (unsigned char)jp_s[jp_i] >= 0x20) jp_i++;
    if (jp_i < jp_n && jp_s[jp_i] == '"') return str_new(jp_s + from, jp_i++ - from); /* no escapes: as it is */
    SB *sb = sb_new();
    GC_ROOT(&sb);
    Str *out = 0;
    sb_bytes(sb, jp_s + from, jp_i - from);
    for (;;) {
        if (jp_i >= jp_n) {
            jp_fail(jp_i, "unterminated string");
            break;
        }
        unsigned char c = (unsigned char)jp_s[jp_i];
        if (c == '"') {
            jp_i++;
            out = sb_finish(sb);
            break;
        }
        if (c < 0x20) {
            jp_fail(jp_i, "control character in a string: write it as an escape");
            break;
        }
        if (c != '\\') {
            sb_byte(sb, c);
            jp_i++;
            continue;
        }
        long esc = jp_i++; /* the backslash: where surrogate problems are reported */
        int e = jp_i < jp_n ? (unsigned char)jp_s[jp_i] : 0;
        if (e == '"' || e == '\\' || e == '/') sb_byte(sb, e);
        else if (e == 'n') sb_byte(sb, '\n');
        else if (e == 't') sb_byte(sb, '\t');
        else if (e == 'r') sb_byte(sb, '\r');
        else if (e == 'b') sb_byte(sb, 8);
        else if (e == 'f') sb_byte(sb, 12);
        else if (e == 'u') {
            jp_i++;
            long hi = jp_hex4();
            if (hi < 0) break;
            if (hi >= 0xDC00 && hi <= 0xDFFF) {
                jp_fail(esc, "unpaired low surrogate");
                break;
            }
            if (hi >= 0xD800 && hi <= 0xDBFF) {
                if (jp_i + 1 >= jp_n || jp_s[jp_i] != '\\' || jp_s[jp_i + 1] != 'u') {
                    jp_fail(esc, "unpaired high surrogate");
                    break;
                }
                jp_i += 2;
                long lo = jp_hex4();
                if (lo < 0) break;
                if (lo < 0xDC00 || lo > 0xDFFF) {
                    jp_fail(esc, "unpaired high surrogate");
                    break;
                }
                hi = 0x10000 + (hi - 0xD800) * 1024 + (lo - 0xDC00);
            }
            sb_utf8(sb, hi);
            continue; /* jp_hex4 moved past the digits */
        } else {
            jp_fail(jp_i, "invalid escape");
            break;
        }
        jp_i++;
    }
    GC_UNROOT(1);
    return out;
}

/* The float of 0.DIGITS times 10^point from the digits of two runs of the text (the integer and
 * fraction parts), at most 19 significant ones: std/json.vo's decimal_float, through the text
 * __text_float reads. */
static long jp_float(int negative, long a0, long a1, long b0, long b1, long point)
{
    unsigned long sign = negative ? 1UL << 63 : 0;
    long sig = 0, first = 0; /* significant digits: from the first that is not zero */
    for (int run = 0; run < 2; run++)
        for (long i = run ? b0 : a0, end = run ? b1 : a1; i < end; i++) {
            first |= jp_s[i] != '0';
            sig += first;
        }
    if (sig > 19) { /* more than a word holds: every digit counts, exactly. Past 699 digits only whether
                       one is not zero matters here (a 700th digit `1`: a halfway point is never that long
                       in a value this code is given, and nothing shorter lies between) */
        char big[700];
        long n = 0, lead = 0;
        int more = 0;
        for (int run = 0; run < 2; run++)
            for (long i = run ? b0 : a0, end = run ? b1 : a1; i < end; i++) {
                if (!n && jp_s[i] == '0') {
                    lead++;
                    continue;
                }
                if (n < 699) big[n++] = jp_s[i];
                else more |= jp_s[i] != '0';
            }
        if (more) big[n++] = '1';
        /* 0.DIGITS * 10^point = DIGITS * 10^(point - their count) */
        return (long)(exact_digits_to_double(big, n, point - lead - n) | sign);
    }
    char d[19];
    long nd = 0;
    int leading = 1;
    for (int run = 0; run < 2; run++)
        for (long i = run ? b0 : a0, end = run ? b1 : a1; i < end && nd < 19; i++) {
            if (leading && jp_s[i] == '0') {
                point--;
                continue;
            }
            leading = 0;
            d[nd++] = jp_s[i];
        }
    if (!nd) return (long)sign;
    if (point > 400) point = 400;
    if (point < -400) return (long)sign;
    char buf[16 + 448] __attribute__((aligned(8)));
    Str *t = (Str *)buf; /* never reaches the heap: std_text_float only reads len and data */
    char *p = t->data;
    if (negative) *p++ = '-';
    if (point <= 0) {
        *p++ = '0';
        *p++ = '.';
        for (long k = 0; k < -point; k++) *p++ = '0';
        for (long k = 0; k < nd; k++) *p++ = d[k];
    } else if (point >= nd) {
        for (long k = 0; k < nd; k++) *p++ = d[k];
        for (long k = nd; k < point; k++) *p++ = '0';
    } else {
        for (long k = 0; k < nd; k++) {
            if (k == point) *p++ = '.';
            *p++ = d[k];
        }
    }
    t->len = p - t->data;
    return std_text_float(t);
}

static long jp_number(void)
{
    long start = jp_i;
    int negative = 0, is_float = 0;
    if (jp_s[jp_i] == '-') {
        negative = 1;
        jp_i++;
    }
    long a0 = jp_i;
    if (jp_i < jp_n && jp_s[jp_i] == '0') {
        jp_i++;
        if (jp_digit()) return jp_fail(jp_i, "a number cannot have leading zeros");
    } else if (jp_digit()) {
        while (jp_digit()) jp_i++;
    } else {
        return jp_fail(jp_i, "expected a digit");
    }
    long a1 = jp_i, b0 = jp_i, b1 = jp_i;
    if (jp_i < jp_n && jp_s[jp_i] == '.') {
        is_float = 1;
        jp_i++;
        if (!jp_digit()) return jp_fail(jp_i, "expected a digit after `.`");
        b0 = jp_i;
        while (jp_digit()) jp_i++;
        b1 = jp_i;
    }
    long exponent = 0;
    if (jp_i < jp_n && (jp_s[jp_i] == 'e' || jp_s[jp_i] == 'E')) {
        is_float = 1;
        jp_i++;
        int exp_negative = 0;
        if (jp_i < jp_n && jp_s[jp_i] == '+') {
            jp_i++;
        } else if (jp_i < jp_n && jp_s[jp_i] == '-') {
            exp_negative = 1;
            jp_i++;
        }
        if (!jp_digit()) return jp_fail(jp_i, "expected a digit in the exponent");
        for (; jp_digit(); jp_i++)
            if (exponent < 100000) exponent = exponent * 10 + (jp_s[jp_i] - '0');
        if (exp_negative) exponent = -exponent;
    }
    long v;
    if (!is_float && !dec_to_int(jp_s + start, a1 - start, &v)) return (long)pg_new_variant(J_INT, v);
    long bits = jp_float(negative, a0, a1, b0, b1, a1 - a0 + exponent);
    union {
        long i;
        double d;
    } u = {bits};
    if (u.d - u.d != 0) return jp_fail(start, "number out of range");
    return (long)pg_new_variant(J_NUM, bits);
}

static long jp_word(char *word, long tag, long pay)
{
    long start = jp_i;
    for (char *w = word; *w; w++, jp_i++)
        if (jp_i >= jp_n || jp_s[jp_i] != *w) {
            SB *sb = sb_new(); /* "expected `word`", at the word's start */
            GC_ROOT(&sb);
            sb_cstr(sb, "expected `");
            sb_cstr(sb, word);
            sb_byte(sb, '`');
            sb_byte(sb, 0);
            jp_fail(start, sb->ptr);
            GC_UNROOT(1);
            return 0;
        }
    return tag == J_NULL ? (long)variant_bare(J_NULL) : (long)pg_new_variant(tag, pay);
}

static long jp_object(long depth)
{
    Map *m = map_new();
    Str *key = 0;
    long v = 0, r = 0;
    GC_ROOT(&m);
    GC_ROOT(&key);
    jp_i++;
    jp_space();
    if (jp_i < jp_n && jp_s[jp_i] == '}') {
        jp_i++;
        r = (long)pg_new_variant(J_OBJECT, (long)m);
    } else {
        for (;;) {
            if (jp_i >= jp_n || jp_s[jp_i] != '"') {
                jp_fail(jp_i, "expected a string key");
                break;
            }
            if (!(key = jp_string())) break;
            jp_space();
            if (jp_i >= jp_n || jp_s[jp_i] != ':') {
                jp_fail(jp_i, "expected `:` after the key");
                break;
            }
            jp_i++;
            jp_space();
            if (!(v = jp_value(depth + 1))) break;
            map_set(m, (long)key, v, t_str); /* a repeated key keeps its place and takes the last value */
            jp_space();
            int c = jp_i < jp_n ? jp_s[jp_i] : 0;
            if (c == '}') {
                jp_i++;
                r = (long)pg_new_variant(J_OBJECT, (long)m);
                break;
            }
            if (c != ',') {
                jp_fail(jp_i, "expected `,` or `}`");
                break;
            }
            jp_i++;
            jp_space();
        }
    }
    GC_UNROOT(2);
    return r;
}

static long jp_array(long depth)
{
    List *xs = list_new(0);
    long v = 0, r = 0;
    GC_ROOT(&xs);
    jp_i++;
    jp_space();
    if (jp_i < jp_n && jp_s[jp_i] == ']') {
        jp_i++;
        r = (long)pg_new_variant(J_ARRAY, (long)xs);
    } else {
        for (;;) {
            if (!(v = jp_value(depth + 1))) break;
            list_push(xs, v);
            jp_space();
            int c = jp_i < jp_n ? jp_s[jp_i] : 0;
            if (c == ']') {
                jp_i++;
                r = (long)pg_new_variant(J_ARRAY, (long)xs);
                break;
            }
            if (c != ',') {
                jp_fail(jp_i, "expected `,` or `]`");
                break;
            }
            jp_i++;
            jp_space();
        }
    }
    GC_UNROOT(1);
    return r;
}

static long jp_value(long depth)
{
    POLL(1);
    if (jp_i >= jp_n) return jp_fail(jp_i, "expected a value, found the end of the text");
    int c = (unsigned char)jp_s[jp_i];
    if ((c == '{' || c == '[') && depth >= JP_DEPTH) return jp_fail(jp_i, "nested more than 512 levels");
    if (c == '{') return jp_object(depth);
    if (c == '[') return jp_array(depth);
    if (c == '"') {
        Str *s = jp_string();
        return s ? (long)pg_new_variant(J_TEXT, (long)s) : 0;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return jp_number();
    if (c == 't') return jp_word("true", J_BOOL, 1);
    if (c == 'f') return jp_word("false", J_BOOL, 0);
    if (c == 'n') return jp_word("null", J_NULL, 0);
    return jp_fail_found(jp_i, "expected a value, found ", 1);
}

/* `__json_parse(s)`: result[Json], ok(the value) or err("line L, column C: what"). */
Variant *json_parse(Str *s, Node *call)
{
    GC_ROOT(&s);
    check_stack(call);
    jp_s = s->data;
    jp_n = s->len;
    jp_i = 0;
    jp_err = 0;
    jp_call = call;
    jp_space();
    long v = jp_value(0);
    GC_ROOT(&v);
    if (v) {
        jp_space();
        if (jp_i < jp_n) v = jp_fail(jp_i, "unexpected text after the value");
    }
    Variant *r = v ? make_result(0, v) : make_result(1, (long)jp_err);
    jp_err = 0;
    GC_UNROOT(2);
    return r;
}

/* ---- writing ---- */

static void jw_line(SB *sb, long pretty, long depth)
{
    if (!pretty) return;
    sb_byte(sb, '\n');
    for (long i = 0; i < depth; i++) sb_cstr(sb, "  ");
}

/* j as JSON text on sb; 0 when it holds a NaN or an infinite number. */
static int jw_value(SB *sb, long j, long pretty, long depth)
{
    check_stack(jp_call);
    POLL(1);
    Variant *jj = (Variant *)j;
    int good = 1;
    GC_ROOT(&j);
    switch ((long)(uint32_t)jj->tag) {
    case J_NULL: sb_cstr(sb, "null"); break;
    case J_BOOL: sb_cstr(sb, jj->pay.i ? "true" : "false"); break;
    case J_INT:
        if (jj->pay.i >= 0) {
            sb_long(sb, jj->pay.i);
        } else {
            Str *t = int_to_str(jj->pay.i); /* (the smallest int has no positive) */
            GC_ROOT(&t);
            sb_str(sb, t);
            GC_UNROOT(1);
        }
        break;
    case J_NUM: {
        union {
            long i;
            double d;
        } u = {jj->pay.i};
        if (u.d - u.d != 0) {
            good = 0;
            break;
        }
        Str *t = std_float_text(jj->pay.i);
        GC_ROOT(&t);
        sb_str(sb, t);
        GC_UNROOT(1);
        break;
    }
    case J_TEXT: json_quote(sb, jj->pay.s); break;
    case J_ARRAY: {
        List *xs = jj->pay.p;
        if (!xs->len) {
            sb_cstr(sb, "[]");
            break;
        }
        sb_byte(sb, '[');
        for (long i = 0; good && i < xs->len; i++) {
            if (i) sb_byte(sb, ',');
            jw_line(sb, pretty, depth + 1);
            good = jw_value(sb, xs->data[i].i, pretty, depth + 1);
        }
        jw_line(sb, pretty, depth);
        sb_byte(sb, ']');
        break;
    }
    default: { /* J_OBJECT */
        Map *m = jj->pay.p;
        if (!m->count) {
            sb_cstr(sb, "{}");
            break;
        }
        sb_byte(sb, '{');
        int first = 1;
        for (long i = 0; good && i < m->used; i++) {
            if (!m->entries[i].live) continue;
            if (!first) sb_byte(sb, ',');
            first = 0;
            jw_line(sb, pretty, depth + 1);
            json_quote(sb, m->entries[i].key.s);
            sb_byte(sb, ':');
            if (pretty) sb_byte(sb, ' ');
            good = jw_value(sb, m->entries[i].val.i, pretty, depth + 1);
        }
        jw_line(sb, pretty, depth);
        sb_byte(sb, '}');
    }
    }
    GC_UNROOT(1);
    return good;
}

/* `__json_write(j, pretty)`: result[str], compact or indented by two spaces. */
Variant *json_write(long j, long pretty, Node *call)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    jp_call = call;
    Str *s = jw_value(sb, j, pretty, 0) ? sb_finish(sb) : 0;
    GC_ROOT(&s);
    Variant *r = s ? make_result(0, (long)s) : make_result(1, (long)str_from_cstr("cannot write a NaN or infinite number as JSON"));
    GC_UNROOT(2);
    return r;
}
