/* SHA-1 (FIPS 180-4) for git object names (spec 061, research R1): a pack's objects are found by it,
 * and the pack's trailer is one. Never used to decide what is trusted: that is the SHA-256 content
 * hash of the package (packages.c). Spec 063: also the WebSocket handshake's accept value, which the
 * protocol defines with it and which proves nothing about trust either. */
#include "vo.h"

static uint32_t rol(uint32_t x, int n)
{
    return x << n | x >> (32 - n);
}

static void sha1_block(Sha1 *c, const unsigned char *p)
{
    uint32_t w[80], a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) f = (b & cc) | (~b & d), k = 0x5a827999;
        else if (i < 40) f = b ^ cc ^ d, k = 0x6ed9eba1;
        else if (i < 60) f = (b & cc) | (b & d) | (cc & d), k = 0x8f1bbcdc;
        else f = b ^ cc ^ d, k = 0xca62c1d6;
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d, d = cc, cc = rol(b, 30), b = a, a = t;
    }
    c->h[0] += a, c->h[1] += b, c->h[2] += cc, c->h[3] += d, c->h[4] += e;
}

void sha1_start(Sha1 *c)
{
    c->h[0] = 0x67452301, c->h[1] = 0xefcdab89, c->h[2] = 0x98badcfe, c->h[3] = 0x10325476, c->h[4] = 0xc3d2e1f0;
    c->total = 0;
}

void sha1_add(Sha1 *c, const char *p, long n)
{
    long have = c->total & 63;
    c->total += n;
    if (have) { /* fill the partial block first */
        long take = 64 - have < n ? 64 - have : n;
        mem_copy((char *)c->buf + have, (char *)p, take);
        p += take, n -= take;
        if (have + take < 64) return;
        sha1_block(c, c->buf);
    }
    for (; n >= 64; p += 64, n -= 64) sha1_block(c, (const unsigned char *)p);
    mem_copy((char *)c->buf, (char *)p, n);
}

void sha1_end(Sha1 *c, char *out)
{
    unsigned long bits = (unsigned long)c->total * 8;
    unsigned char pad[72] = {0x80};
    long have = c->total & 63, n = (have < 56 ? 56 : 120) - have;
    for (int i = 0; i < 8; i++) pad[n + i] = (unsigned char)(bits >> (56 - 8 * i));
    sha1_add(c, (char *)pad, n + 8);
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++) out[4 * i + k] = (char)(c->h[i] >> (24 - 8 * k));
}
