/* JSON-RPC pieces shared by `vo mcp` (src/mcp.c) and `vo lsp` (src/lsp.c): a check of UTF-8 and a JSON
 * reader that works in place on a NUL-terminated message, allocating nothing. A message is checked once
 * with js_skip; the other functions then assume it is well formed. */
#include "vo.h"

#define MAX_DEPTH 64 /* nesting of a message */

/* The length of the well-formed UTF-8 sequence at p (4 bytes may be read; a NUL ends any sequence),
 * or 0: no overlong forms, no surrogates, nothing above U+10FFFF. */
int utf8_seq(unsigned char *p, long n)
{
    unsigned c = p[0];
    if (c < 0x80) return 1;
    if (c < 0xC2) return 0;
    if (c < 0xE0) return n >= 2 && (p[1] & 0xC0) == 0x80 ? 2 : 0;
    if (c < 0xF0) {
        if (n < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0;
        if ((c == 0xE0 && p[1] < 0xA0) || (c == 0xED && p[1] >= 0xA0)) return 0;
        return 3;
    }
    if (c < 0xF5) {
        if (n < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return 0;
        if ((c == 0xF0 && p[1] < 0x90) || (c == 0xF4 && p[1] >= 0x90)) return 0;
        return 4;
    }
    return 0;
}

char *js_ws(char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_hex(char c)
{
    return is_digit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f');
}

static int starts(char *p, char *lit)
{
    while (*lit)
        if (*p++ != *lit++) return 0;
    return 1;
}

/* The end of the JSON value at p, or 0 when it is not one. */
char *js_skip(char *p, int depth)
{
    if (depth > MAX_DEPTH) return 0;
    if (*p == '"') {
        for (p++;;) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') return p + 1;
            if (c < 0x20) return 0; /* includes the end of the line */
            if (c == '\\') {
                char e = p[1];
                if (e == 'u') {
                    if (!is_hex(p[2]) || !is_hex(p[3]) || !is_hex(p[4]) || !is_hex(p[5])) return 0;
                    p += 6;
                } else if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' ||
                           e == 't') {
                    p += 2;
                } else {
                    return 0;
                }
            } else {
                int k = utf8_seq((unsigned char *)p, 4);
                if (!k) return 0;
                p += k;
            }
        }
    }
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        int object = *p == '{';
        p = js_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (object) {
                if (*p != '"') return 0;
                p = js_skip(p, depth);
                if (!p) return 0;
                p = js_ws(p);
                if (*p != ':') return 0;
                p = js_ws(p + 1);
            }
            p = js_skip(p, depth + 1);
            if (!p) return 0;
            p = js_ws(p);
            if (*p == close) return p + 1;
            if (*p != ',') return 0;
            p = js_ws(p + 1);
        }
    }
    if (starts(p, "true") || starts(p, "null")) return p + 4;
    if (starts(p, "false")) return p + 5;
    if (*p == '-') p++;
    if (*p == '0') {
        p++;
    } else {
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
    }
    if (*p == '.') {
        if (!is_digit(*++p)) return 0;
        while (is_digit(*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
    }
    return p;
}

/* Steps through the members of a checked object: *p starts at js_ws(object + 1); 0 at the end, else
 * the key (at its quote) and the value. */
int js_member(char **p, char **key, char **val)
{
    if (**p == '}') return 0;
    *key = *p;
    char *q = js_ws(js_skip(*p, 0));
    *val = js_ws(q + 1);
    q = js_ws(js_skip(*val, 0));
    *p = *q == ',' ? js_ws(q + 1) : q;
    return 1;
}

/* 1 when the checked JSON string at p is exactly lit (n bytes), compared as written (no escapes). */
int js_is(char *p, char *lit, long n)
{
    return *p == '"' && mem_eq(p + 1, lit, n) && p[1 + n] == '"';
}

/* The value of key in the checked object, or 0. */
char *js_get(char *object, char *key)
{
    if (!object || *object != '{') return 0;
    char *p = js_ws(object + 1), *k, *v;
    long n = cstr_len(key);
    while (js_member(&p, &k, &v))
        if (js_is(k, key, n)) return v;
    return 0;
}

static int hex_val(char c)
{
    return is_digit(c) ? c - '0' : (c | 32) - 'a' + 10;
}

static unsigned hex4(char *p)
{
    return (unsigned)(hex_val(p[0]) << 12 | hex_val(p[1]) << 8 | hex_val(p[2]) << 4 | hex_val(p[3]));
}

/* The checked JSON string at p, unescaped into dst with a NUL after it; the length. A lone surrogate
 * becomes U+FFFD. Unescaping never grows a string: dst needs as many bytes as the string has. */
long js_unescape(char *p, char *dst)
{
    char *d = dst;
    for (p++; *p != '"';) {
        if (*p != '\\') {
            *d++ = *p++;
            continue;
        }
        char e = p[1];
        p += 2;
        if (e != 'u') {
            *d++ = e == 'b' ? '\b' : e == 'f' ? '\f' : e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : e;
            continue;
        }
        unsigned c = hex4(p);
        p += 4;
        if (c >= 0xD800 && c < 0xDC00 && p[0] == '\\' && p[1] == 'u' && is_hex(p[2]) && is_hex(p[3]) &&
            is_hex(p[4]) && is_hex(p[5]) && hex4(p + 2) >= 0xDC00 && hex4(p + 2) < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (hex4(p + 2) - 0xDC00);
            p += 6;
        } else if (c >= 0xD800 && c < 0xE000) {
            c = 0xFFFD;
        }
        if (c < 0x80) {
            *d++ = (char)c;
        } else if (c < 0x800) {
            *d++ = (char)(0xC0 | c >> 6);
            *d++ = (char)(0x80 | (c & 63));
        } else if (c < 0x10000) {
            *d++ = (char)(0xE0 | c >> 12);
            *d++ = (char)(0x80 | (c >> 6 & 63));
            *d++ = (char)(0x80 | (c & 63));
        } else {
            *d++ = (char)(0xF0 | c >> 18);
            *d++ = (char)(0x80 | (c >> 12 & 63));
            *d++ = (char)(0x80 | (c >> 6 & 63));
            *d++ = (char)(0x80 | (c & 63));
        }
    }
    *d = 0;
    return d - dst;
}
