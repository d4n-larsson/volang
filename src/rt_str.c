/* String builtins and conversions (ported from src/rt_str.s, spec 008). Runtime code: a heap
 * reference held across an allocation is registered with GC_ROOT (research R4). */
#include "vo.h"

typedef union {
    long i;
    double d;
} Bits;

Str *str_concat(Str *a, Str *b)
{
#ifdef GC_STRESS_BUILD
    gc_check(a, OBJ_STR); /* stress build: a live OBJ_STR (spec 003 FR-011) */
    gc_check(b, OBJ_STR);
#endif
    long n = a->len + b->len;
    GC_ROOT(&a);
    GC_ROOT(&b);
    Str *s = alloc_obj(OBJ_STR, n + 9);
    GC_UNROOT(2);
    s->len = n;
    mem_copy(s->data, a->data, a->len);
    mem_copy(s->data + a->len, b->data, b->len);
    return s;
}

/* 1 when byte i of s is a UTF-8 continuation byte (i < len). */
int is_cont(Str *s, long i)
{
    return i < s->len && ((unsigned char)s->data[i] & 0xC0) == 0x80;
}

Str *rt_slice_str(Str *s, long a, long b, Node *node)
{
    long len = s->len;
    if (a < 0 || a > len) rt_index_error(node, a, len);
    if (b < a || b > len) rt_index_error(node, b, len);
    long split = -1; /* R0007 on the boundary that splits a character */
    if (is_cont(s, a))
        split = a;
    else if (is_cont(s, b))
        split = b;
    if (split >= 0)
        rt_raise("R0007", msg("Slice boundary $1 splits a UTF-8 character.", int_to_str(split), 0, 0), node);
    return str_new(s->data + a, b - a);
}

/* Byte offset of the first occurrence at or after from, or -1. */
long str_find_from(Str *s, Str *sub, long from)
{
    long last = s->len - sub->len; /* last start */
    for (long i = from; i <= last; i++) {
        long k = 0;
        while (k < sub->len && s->data[i + k] == sub->data[k]) k++;
        if (k >= sub->len) return i;
        POLL(k + 1); /* one work unit per byte compared */
    }
    return -1;
}

long rt_find(Str *s, Str *sub)
{
    return str_find_from(s, sub, 0);
}

List *rt_split(Str *s, Str *sep, Node *node)
{
    if (!sep->len) rt_raise_c("R0007", "`split` needs a non-empty separator.", node);
    GC_ROOT(&s);
    GC_ROOT(&sep);
    List *list = list_new(4);
    GC_ROOT(&list);
    long start = 0; /* piece start */
    for (;;) {
        POLL(1);
        long at = str_find_from(s, sep, start);
        if (at < 0) break;
        list_push(list, (long)str_new(s->data + start, at - start));
        start = at + sep->len;
    }
    list_push(list, (long)str_new(s->data + start, s->len - start));
    GC_UNROOT(3);
    return list;
}

Str *rt_join(List *list, Str *sep)
{
    GC_ROOT(&list);
    GC_ROOT(&sep);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    for (long i = 0;; i++) {
        POLL(1);
        if (i >= list->len) break;
        if (i) sb_str(sb, sep);
        sb_str(sb, list->data[i].s);
    }
    GC_UNROOT(3);
    return sb_finish(sb);
}

int is_space(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

Str *rt_trim(Str *s)
{
    long a = 0, b = s->len;
    while (a < b && is_space((unsigned char)s->data[a])) a++;
    while (b > a && is_space((unsigned char)s->data[b - 1])) b--;
    return str_new(s->data + a, b - a);
}

List *rt_chars(Str *s)
{
    GC_ROOT(&s);
    List *list = list_new(s->len);
    GC_ROOT(&list);
    for (long i = 0; i < s->len;) {
        long k = utf8_seq_len(s->data + i, s->len - i);
        if (!k) k = 1;
        list_push(list, (long)str_new(s->data + i, k));
        i += k;
    }
    GC_UNROOT(2);
    return list;
}

Str *bool_str(long b)
{
    return str_from_cstr(b ? "true" : "false");
}

/* result[int]: ok(value) or err("invalid int: <text>"). */
Variant *rt_parse_int(Str *s)
{
    long v;
    if (!dec_to_int(s->data, s->len, &v)) return make_result(0, v);
    return make_result(1, (long)msg("invalid int: $1", s, 0, 0));
}

Variant *rt_parse_float(Str *s)
{
    long v;
    if (!float_parse(s->data, s->len, &v)) return make_result(0, v);
    return make_result(1, (long)msg("invalid float: $1", s, 0, 0));
}

/* Truncates toward zero; R0007 for NaN, infinities and values outside the int range. */
long rt_int_of_float(long bits, Node *node)
{
    Bits x = {.i = bits};
    if (x.d == x.d && !(9223372036854775808.0 <= x.d) && !(x.d < -9223372036854775808.0)) return (long)x.d;
    rt_raise("R0007", msg("Cannot convert $1 to int.", float_to_str(bits), 0, 0), node);
    return 0;
}

long rt_float_of_int(long i)
{
    Bits x = {.d = (double)i};
    return x.i;
}

long rt_abs(long v, int kind, Node *node)
{
    if (kind == TY_FLOAT) return (long)((unsigned long)v << 1 >> 1); /* clear the sign bit */
    if (v >= 0) return v;
    if (v == (long)(1UL << 63)) { /* abs(MIN) overflows */
        rt_overflow(node);
        return v;
    }
    return -v;
}

long rt_minmax(long a, long b, int kind, int want_max)
{
    int b_less; /* b < a */
    if (kind == TY_FLOAT) {
        Bits x = {.i = a}, y = {.i = b};
        b_less = y.d < x.d;
    } else if (kind == TY_STR) {
        b_less = str_cmp((Str *)b, (Str *)a) < 0;
    } else if (kind == TY_RECORD) { /* spec 014 */
        b_less = elem_less(b, a, kind);
    } else {
        b_less = b < a;
    }
    return (b_less ^ want_max) ? b : a; /* min: b when b < a; max: b when not (b < a) */
}

/* ASCII letters to upper (1) or lower (0) case; other bytes unchanged. */
Str *rt_case(Str *s, int upper)
{
    Str *r = str_new(s->data, s->len);
    for (long i = 0; i < r->len; i++) {
        int c = (unsigned char)r->data[i];
        if (upper && c >= 'a' && c <= 'z') c -= 32;
        if (!upper && c >= 'A' && c <= 'Z') c += 32;
        r->data[i] = (char)c;
    }
    return r;
}

/* The code point of a one-character string (R0007 otherwise). */
long rt_ord(Str *s, Node *node)
{
    long k = utf8_seq_len(s->data, s->len);
    if (k && k == s->len) {
        int c = (unsigned char)s->data[0];
        if (k == 1) return c;
        long cp = c & (k == 2 ? 0x1f : k == 3 ? 0x0f : 0x07);
        for (long i = 1; i < k; i++) cp = cp << 6 | ((unsigned char)s->data[i] & 0x3f); /* continuation bytes */
        return cp;
    }
    GC_ROOT(&s); /* R0007 with the string in vo syntax */
    SB *sb = sb_new();
    GC_ROOT(&sb);
    render_str_lit(sb, s);
    GC_UNROOT(2);
    rt_raise("R0007", msg("`ord` needs exactly one character, got $1.", sb_finish(sb), 0, 0), node);
    return 0;
}

/* The one-character string for a code point (R0007 for negatives, surrogates, > 0x10FFFF). */
Str *rt_chr(long cp, Node *node)
{
    if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        rt_raise("R0007", msg("`chr` needs a Unicode code point, got $1.", int_to_str(cp), 0, 0), node);
        return 0;
    }
    char buf[4];
    long n;
    if (cp < 0x80) {
        buf[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        buf[0] = (char)(cp >> 6 | 0xC0);
        buf[1] = (char)((cp & 0x3f) | 0x80);
        n = 2;
    } else if (cp < 0x10000) {
        buf[0] = (char)(cp >> 12 | 0xE0);
        buf[1] = (char)((cp >> 6 & 0x3f) | 0x80);
        buf[2] = (char)((cp & 0x3f) | 0x80);
        n = 3;
    } else {
        buf[0] = (char)(cp >> 18 | 0xF0);
        buf[1] = (char)((cp >> 12 & 0x3f) | 0x80);
        buf[2] = (char)((cp >> 6 & 0x3f) | 0x80);
        buf[3] = (char)((cp & 0x3f) | 0x80);
        n = 4;
    }
    return str_new(buf, n);
}
