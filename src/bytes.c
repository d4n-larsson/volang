/* std.bytes's primitives (spec 021): conversions between bytes, str and list[int], hex and base64.
 * A bytes value is a Str object whose data may be any bytes (research R1); only the checker tells the
 * two types apart. Errors that a function cannot return as an `err` stop with R0007 at the user's
 * call (std_user_call). */
#include "vo.h"

static const char hexd[] = "0123456789abcdef";
static const char b64d[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* A new bytes value of n bytes, zeroed (the arena is zeroed), to be filled in. */
static Str *bytes_alloc(long n)
{
    Str *s = alloc_obj(OBJ_STR, n + 9);
    s->len = n;
    return s;
}

/* to_text: ok(the same bytes as str) when they are well-formed UTF-8, else err naming the first bad byte. */
Variant *bytes_to_text(Str *b)
{
    long bad = utf8_validate(b->data, b->len);
    if (bad < 0) return make_result(0, (long)b);
    return make_result(1, (long)msg("not UTF-8 at byte $1", int_to_str(bad), 0, 0));
}

/* from_ints: R0007 for an element outside 0 to 255, naming its index and value. */
Str *bytes_from_ints(List *xs, Node *node)
{
    for (long i = 0; i < xs->len; i++) {
        long v = xs->data[i].i;
        if (v < 0 || v > 255) {
            Str *a = int_to_str(i);
            GC_ROOT(&a); /* (rt_raise does not return: no unroot) */
            Str *b = int_to_str(v);
            rt_raise("R0007", msg("Byte $1 is $2, not 0 to 255.", a, b, 0), std_user_call(node));
        }
    }
    GC_ROOT(&xs);
    Str *s = bytes_alloc(xs->len);
    for (long i = 0; i < xs->len; i++) s->data[i] = (char)(unsigned char)xs->data[i].i;
    GC_UNROOT(1);
    return s;
}

List *bytes_to_ints(Str *b)
{
    GC_ROOT(&b);
    List *l = list_new(b->len);
    for (long i = 0; i < b->len; i++) list_push(l, (unsigned char)b->data[i]);
    GC_UNROOT(1);
    return l;
}

Str *bytes_to_hex(Str *b)
{
    GC_ROOT(&b);
    Str *s = bytes_alloc(b->len * 2);
    for (long i = 0; i < b->len; i++) {
        unsigned char c = (unsigned char)b->data[i];
        s->data[2 * i] = hexd[c >> 4];
        s->data[2 * i + 1] = hexd[c & 15];
    }
    GC_UNROOT(1);
    return s;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static Variant *err_at(char *tmpl, long pos)
{
    return make_result(1, (long)msg(tmpl, int_to_str(pos), 0, 0));
}

/* from_hex: either case; err for a character that is not a hex digit (its position) or an odd count. */
Variant *bytes_from_hex(Str *s)
{
    for (long i = 0; i < s->len; i++)
        if (hex_val(s->data[i]) < 0) return err_at("not hex at position $1", i);
    if (s->len % 2) return make_result(1, (long)str_from_cstr("odd number of hex digits"));
    GC_ROOT(&s);
    Str *b = bytes_alloc(s->len / 2);
    for (long i = 0; i < b->len; i++)
        b->data[i] = (char)(hex_val(s->data[2 * i]) * 16 + hex_val(s->data[2 * i + 1]));
    GC_UNROOT(1);
    return make_result(0, (long)b);
}

/* RFC 4648 base64, standard alphabet, padded. */
Str *bytes_to_base64(Str *b)
{
    GC_ROOT(&b);
    long n = b->len;
    Str *s = bytes_alloc((n + 2) / 3 * 4);
    char *w = s->data;
    const unsigned char *p = (const unsigned char *)b->data;
    long i = 0;
    for (; i + 3 <= n; i += 3) {
        unsigned long v = (unsigned long)p[i] << 16 | (unsigned long)p[i + 1] << 8 | p[i + 2];
        *w++ = b64d[v >> 18];
        *w++ = b64d[(v >> 12) & 63];
        *w++ = b64d[(v >> 6) & 63];
        *w++ = b64d[v & 63];
    }
    if (n - i == 1) {
        unsigned long v = (unsigned long)p[i] << 16;
        *w++ = b64d[v >> 18];
        *w++ = b64d[(v >> 12) & 63];
        *w++ = '=';
        *w++ = '=';
    } else if (n - i == 2) {
        unsigned long v = (unsigned long)p[i] << 16 | (unsigned long)p[i + 1] << 8;
        *w++ = b64d[v >> 18];
        *w++ = b64d[(v >> 12) & 63];
        *w++ = b64d[(v >> 6) & 63];
        *w++ = '=';
    }
    GC_UNROOT(1);
    return s;
}

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* from_base64: padded standard base64 only. err for a bad character (its position), padding anywhere
 * but the last two places of the last group, or a length that is not a multiple of 4. */
Variant *bytes_from_base64(Str *s)
{
    long n = s->len, pad = 0;
    if (n >= 1 && s->data[n - 1] == '=') pad++;
    if (n >= 2 && s->data[n - 2] == '=') pad++;
    for (long i = 0; i < n - pad; i++)
        if (b64_val(s->data[i]) < 0) return err_at("not base64 at position $1", i);
    if (n % 4) return make_result(1, (long)str_from_cstr("base64 length is not a multiple of 4"));
    GC_ROOT(&s);
    Str *b = bytes_alloc(n / 4 * 3 - pad);
    long o = 0;
    for (long i = 0; i < n; i += 4) {
        unsigned long v = 0;
        for (int k = 0; k < 4; k++) {
            int d = s->data[i + k] == '=' ? 0 : b64_val(s->data[i + k]);
            v = v << 6 | (unsigned long)d;
        }
        if (o < b->len) b->data[o++] = (char)(v >> 16);
        if (o < b->len) b->data[o++] = (char)(v >> 8);
        if (o < b->len) b->data[o++] = (char)v;
    }
    GC_UNROOT(1);
    return make_result(0, (long)b);
}

/* ---- strict text (spec 021, research R5): every str is well-formed UTF-8 ---- */

/* An ok(str) result whose text is not UTF-8 becomes err(tmpl), $1 the offset of the first bad byte. */
Variant *strict_result(Variant *r, char *tmpl)
{
    if (r->tag != 0 || !r->pay.s) return r;
    long bad = utf8_validate(r->pay.s->data, r->pay.s->len);
    if (bad < 0) return r;
    return make_result(1, (long)msg(tmpl, int_to_str(bad), 0, 0));
}

/* R0007 at node for a str that is not UTF-8 (tmpl: $1 the offset); otherwise s. */
Str *strict_str(Str *s, char *tmpl, Node *node)
{
    long bad = utf8_validate(s->data, s->len);
    if (bad >= 0) rt_raise("R0007", msg(tmpl, int_to_str(bad), 0, 0), node);
    return s;
}

/* args(): R0007 at node naming the first argument that is not UTF-8. */
List *strict_args(List *l, Node *node)
{
    for (long i = 0; i < l->len; i++) {
        Str *a = l->data[i].s;
        long bad = utf8_validate(a->data, a->len);
        if (bad >= 0) {
            Str *k = int_to_str(i);
            GC_ROOT(&k); /* (rt_raise does not return: no unroot) */
            rt_raise("R0007", msg("Argument $1 is not UTF-8 at byte $2.", k, int_to_str(bad), 0), node);
        }
    }
    return l;
}
