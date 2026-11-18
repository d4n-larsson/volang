/* Memory and string helpers, the growable byte builder, UTF-8, JSON escaping, message templates
 * (ported from src/text.s, spec 008 step 2). */
#include "vo.h"

/* Forward byte copy, as the assembly did (callers rely on nothing more; overlapping ranges copy
 * forward). */
void mem_copy(char *dst, char *src, long n)
{
    for (long i = 0; i < n; i++) dst[i] = src[i];
}

int mem_eq(char *a, char *b, long n)
{
    for (long i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

long cstr_len(char *s)
{
    long n = 0;
    while (s[n]) n++;
    return n;
}

Str *str_new(char *p, long n)
{
    GC_ROOT(&p); /* p may point into a collected string */
    Str *s = alloc_obj(OBJ_STR, n + 9); /* len field + bytes + NUL (the arena is zeroed) */
    GC_UNROOT(1);
    s->len = n;
    mem_copy(s->data, p, n);
    return s;
}

Str *str_from_cstr(char *s)
{
    return str_new(s, cstr_len(s));
}

int str_eq(Str *a, Str *b)
{
    if (a->len != b->len) return 0;
    return mem_eq(a->data, b->data, a->len);
}

int str_eq_bytes(Str *s, char *p, long n)
{
    if (s->len != n) return 0;
    return mem_eq(s->data, p, n);
}

/* Byte-wise comparison: -1, 0 or 1. */
int str_cmp(Str *a, Str *b)
{
    unsigned long la = (unsigned long)a->len, lb = (unsigned long)b->len;
    unsigned long n = la < lb ? la : lb;
    const unsigned char *x = (const unsigned char *)a->data, *y = (const unsigned char *)b->data;
    for (unsigned long i = 0; i < n; i++) {
        if (x[i] < y[i]) return -1;
        if (x[i] > y[i]) return 1;
    }
    if (la < lb) return -1;
    if (la > lb) return 1;
    return 0;
}

SB *sb_new(void)
{
    SB *sb = alloc(SB_SIZE);
    GC_ROOT(&sb);
    sb->ptr = alloc_bytes(64);
    GC_UNROOT(1);
    sb->len = 0;
    sb->cap = 64;
    return sb;
}

/* Makes room for n more bytes. */
void sb_reserve(SB *sb, long n)
{
    unsigned long need = (unsigned long)(sb->len + n);
    if (need <= (unsigned long)sb->cap) return;
    unsigned long cap = (unsigned long)sb->cap * 2;
    if (cap < need) cap = need;
    GC_ROOT(&sb);
    char *p = alloc_bytes((long)cap);
    GC_UNROOT(1);
    mem_copy(p, sb->ptr, sb->len);
    sb->ptr = p;
    sb->cap = (long)cap;
}

void sb_byte(SB *sb, int b)
{
    sb_reserve(sb, 1);
    sb->ptr[sb->len++] = (char)b;
}

void sb_bytes(SB *sb, char *p, long n)
{
    rt_budget -= (long)((unsigned long)n >> 12) + 1; /* one work unit per 4 KiB copied */
    if (rt_budget <= 0) rt_poll();
    GC_ROOT(&p);
    sb_reserve(sb, n);
    GC_UNROOT(1);
    char *dst = sb->ptr + sb->len;
    sb->len += n;
    mem_copy(dst, p, n);
}

void sb_cstr(SB *sb, char *s)
{
    sb_bytes(sb, s, cstr_len(s));
}

void sb_str(SB *sb, Str *str)
{
    if (str) sb_bytes(sb, str->data, str->len);
}

Str *sb_finish(SB *sb)
{
    return str_new(sb->ptr, sb->len);
}

/* Length of the well-formed UTF-8 sequence starting at p (1-4), or 0 if it is not one: Unicode's
 * Table 3-7, so overlong forms, surrogates (U+D800-U+DFFF) and code points above U+10FFFF are refused
 * (spec 021; before, only lead and continuation bytes were checked). */
long utf8_seq_len(char *p, long n)
{
    if (n <= 0) return 0;
    unsigned c = (unsigned char)p[0];
    long len;
    unsigned lo = 0x80, hi = 0xBF; /* the range of the second byte */
    if (c < 0x80) return 1;
    if (c < 0xC2) return 0;
    if (c < 0xE0) {
        len = 2;
    } else if (c < 0xF0) {
        len = 3;
        if (c == 0xE0) lo = 0xA0; /* no overlong 3-byte forms */
        if (c == 0xED) hi = 0x9F; /* no surrogates */
    } else if (c < 0xF5) {
        len = 4;
        if (c == 0xF0) lo = 0x90; /* no overlong 4-byte forms */
        if (c == 0xF4) hi = 0x8F; /* nothing above U+10FFFF */
    } else {
        return 0;
    }
    if (len > n) return 0;
    unsigned c1 = (unsigned char)p[1];
    if (c1 < lo || c1 > hi) return 0;
    for (long i = 2; i < len; i++)
        if (((unsigned char)p[i] & 0xC0) != 0x80) return 0;
    return len;
}

/* -1 when valid, otherwise the offset of the first invalid byte. */
long utf8_validate(char *p, long n)
{
    long i = 0;
    while (i < n) {
        long k = utf8_seq_len(p + i, n - i);
        if (k == 0) return i;
        i += k;
    }
    return -1;
}

long cp_count(char *p, long n)
{
    long count = 0;
    for (; n > 0; n--, p++)
        if (((unsigned char)*p & 0xC0) != 0x80) count++;
    return count;
}

static const char hexdig[] = "0123456789abcdef";

void json_escape_into(SB *sb, char *p, long n)
{
    GC_ROOT(&p);
    GC_ROOT(&sb);
    while (n > 0) {
        unsigned c = (unsigned char)*p;
        if (c >= 0x80) {
            long k = utf8_seq_len(p, n);
            if (k == 0) {
                sb_cstr(sb, "\\ufffd");
                p++;
                n--;
            } else {
                sb_bytes(sb, p, k);
                p += k;
                n -= k;
            }
            continue;
        }
        if (c == '"' || c == '\\' || c == '\n' || c == '\t') {
            sb_byte(sb, '\\');
            sb_byte(sb, c == '\n' ? 'n' : c == '\t' ? 't' : (int)c);
        } else if (c < 0x20) {
            sb_byte(sb, '\\');
            sb_byte(sb, 'u');
            sb_byte(sb, '0');
            sb_byte(sb, '0');
            sb_byte(sb, hexdig[c >> 4]);
            sb_byte(sb, hexdig[c & 15]);
        } else {
            sb_byte(sb, (int)c);
        }
        p++;
        n--;
    }
    GC_UNROOT(2);
}

/* Appends str as a quoted, escaped JSON string. */
void json_str_into(SB *sb, Str *str)
{
    GC_ROOT(&str);
    sb_byte(sb, '"');
    if (str) json_escape_into(sb, str->data, str->len);
    sb_byte(sb, '"');
    GC_UNROOT(1);
}

/* Builds a str from tmpl, replacing $1, $2 and $3 with the given str objects. */
Str *msg(char *tmpl, Str *a1, Str *a2, Str *a3)
{
    GC_ROOT(&a1);
    GC_ROOT(&a2);
    GC_ROOT(&a3);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    for (char *t = tmpl; *t;) {
        if (t[0] == '$' && t[1] >= '1' && t[1] <= '3') {
            sb_str(sb, t[1] == '1' ? a1 : t[1] == '2' ? a2 : a3);
            t += 2;
        } else {
            sb_byte(sb, (unsigned char)*t);
            t++;
        }
    }
    Str *s = sb_finish(sb);
    GC_UNROOT(4);
    return s;
}
