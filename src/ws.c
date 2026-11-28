/* std.websocket's one primitive (spec 063, research R4): what vo code cannot do fast or at all.
 * __ws(op, a, b):
 *   1 + 8 * n: n key bytes (1 to 16) from the system's random source, drawn and recorded as random_bytes'
 *              are (io_entropy) but without the entropy effect: they mask frames and make the handshake
 *              key, never reach the program and change nothing it sees (as TLS's own randomness);
 *   2: a XOR the 4-byte key b, repeated (masking and unmasking are the same);
 *   3: the 20-byte SHA-1 of a (the handshake's Sec-WebSocket-Accept; RFC 6455 section 4.2.2). */
#include "vo.h"

Str *rt_ws(long op, Str *a, Str *b, Node *call)
{
    if ((op & 7) == 1) {
        long n = op >> 3;
        if (n < 1 || n > 16) rt_raise("R0007", str_from_cstr("__ws: 1 to 16 key bytes."), std_user_call(call));
        return io_entropy(n, call);
    }
    if (op == 2) {
        if (b->len != 4) rt_raise("R0007", str_from_cstr("__ws: a key of 4 bytes."), std_user_call(call));
        GC_ROOT(&a);
        GC_ROOT(&b);
        Str *out = str_new(a->data, a->len);
        GC_UNROOT(2);
        unsigned char *p = (unsigned char *)out->data, *k = (unsigned char *)b->data;
        for (long i = 0; i < out->len; i++) p[i] ^= k[i & 3];
        return out;
    }
    if (op == 3) {
        Sha1 c;
        char digest[20];
        sha1_start(&c);
        sha1_add(&c, a->data, a->len);
        sha1_end(&c, digest);
        return str_new(digest, 20);
    }
    rt_raise("R0007", str_from_cstr("__ws: no such operation."), std_user_call(call));
}
