/* SHA-256 (FIPS 180-4) for package content hashes (spec 007 research R8; ported from src/sha256.s,
 * spec 008). One hash at a time: sha256_init, sha256_update (any number of times), sha256_final; or
 * sha256 for one buffer. */
#include "vo.h"

static const uint32_t sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};
static const uint32_t sha_iv[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static Sha256 sha_one; /* the context of sha256_init/update/final (package hashes, vo build) */

static uint32_t rotr(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

void sha256_start(Sha256 *c)
{
    for (int i = 0; i < 8; i++) c->h[i] = sha_iv[i];
    c->buffered = 0;
    c->total = 0;
}

/* Compresses one 64-byte block into the state hh. */
void sha256_compress(uint32_t *hh, const unsigned char *block)
{
    uint32_t w[64];
    for (int t = 0; t < 16; t++)
        w[t] = (uint32_t)block[4 * t] << 24 | (uint32_t)block[4 * t + 1] << 16 |
               (uint32_t)block[4 * t + 2] << 8 | block[4 * t + 3];
    for (int t = 16; t < 64; t++) {
        uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    uint32_t a = hh[0], b = hh[1], cc = hh[2], d = hh[3];
    uint32_t e = hh[4], f = hh[5], g = hh[6], h = hh[7];
    for (int t = 0; t < 64; t++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + sha_k[t] + w[t];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = cc;
        cc = b;
        b = a;
        a = t1 + t2;
    }
    hh[0] += a;
    hh[1] += b;
    hh[2] += cc;
    hh[3] += d;
    hh[4] += e;
    hh[5] += f;
    hh[6] += g;
    hh[7] += h;
}

static void sha256_block(Sha256 *c)
{
    sha256_compress(c->h, c->buf);
}

/* Appends one byte (sha256_add and the padding). */
static void sha256_byte(Sha256 *c, unsigned char b)
{
    c->buf[c->buffered++] = b;
    if (c->buffered == 64) {
        c->buffered = 0;
        sha256_block(c);
    }
}

void sha256_add(Sha256 *c, char *p, long n)
{
    c->total += (unsigned long)n;
    for (long i = 0; i < n; i++) sha256_byte(c, (unsigned char)p[i]);
}

/* Pads, and writes the 32-byte digest to out. */
void sha256_end(Sha256 *c, char *out)
{
    unsigned long bits = c->total << 3;
    sha256_byte(c, 0x80);
    while (c->buffered != 56) sha256_byte(c, 0);
    for (int shift = 56; shift >= 0; shift -= 8) sha256_byte(c, (unsigned char)(bits >> shift));
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (char)(c->h[i] >> 24);
        out[4 * i + 1] = (char)(c->h[i] >> 16);
        out[4 * i + 2] = (char)(c->h[i] >> 8);
        out[4 * i + 3] = (char)c->h[i];
    }
}

void sha256_init(void)
{
    sha256_start(&sha_one);
}

void sha256_update(char *p, long n)
{
    sha256_add(&sha_one, p, n);
}

void sha256_final(char *out)
{
    sha256_end(&sha_one, out);
}

void sha256(char *p, long n, char *out)
{
    Sha256 c;
    sha256_start(&c);
    sha256_add(&c, p, n);
    sha256_end(&c, out);
}
