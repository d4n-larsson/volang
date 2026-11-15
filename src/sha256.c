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

static uint32_t sha_h[8];
static unsigned char sha_buf[64];
static long sha_buffered;
static unsigned long sha_total;

static uint32_t rotr(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

void sha256_init(void)
{
    for (int i = 0; i < 8; i++) sha_h[i] = sha_iv[i];
    sha_buffered = 0;
    sha_total = 0;
}

/* Compresses sha_buf into sha_h. */
static void sha256_block(void)
{
    uint32_t w[64];
    for (int t = 0; t < 16; t++)
        w[t] = (uint32_t)sha_buf[4 * t] << 24 | (uint32_t)sha_buf[4 * t + 1] << 16 |
               (uint32_t)sha_buf[4 * t + 2] << 8 | sha_buf[4 * t + 3];
    for (int t = 16; t < 64; t++) {
        uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    uint32_t a = sha_h[0], b = sha_h[1], c = sha_h[2], d = sha_h[3];
    uint32_t e = sha_h[4], f = sha_h[5], g = sha_h[6], h = sha_h[7];
    for (int t = 0; t < 64; t++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + sha_k[t] + w[t];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    sha_h[0] += a;
    sha_h[1] += b;
    sha_h[2] += c;
    sha_h[3] += d;
    sha_h[4] += e;
    sha_h[5] += f;
    sha_h[6] += g;
    sha_h[7] += h;
}

/* Appends one byte (sha256_update and the padding). */
static void sha256_byte(unsigned char b)
{
    sha_buf[sha_buffered++] = b;
    if (sha_buffered == 64) {
        sha_buffered = 0;
        sha256_block();
    }
}

void sha256_update(char *p, long n)
{
    sha_total += (unsigned long)n;
    for (long i = 0; i < n; i++) sha256_byte((unsigned char)p[i]);
}

/* Pads, and writes the 32-byte digest to out. */
void sha256_final(char *out)
{
    unsigned long bits = sha_total << 3;
    sha256_byte(0x80);
    while (sha_buffered != 56) sha256_byte(0);
    for (int shift = 56; shift >= 0; shift -= 8) sha256_byte((unsigned char)(bits >> shift));
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (char)(sha_h[i] >> 24);
        out[4 * i + 1] = (char)(sha_h[i] >> 16);
        out[4 * i + 2] = (char)(sha_h[i] >> 8);
        out[4 * i + 3] = (char)sha_h[i];
    }
}

void sha256(char *p, long n, char *out)
{
    sha256_init();
    sha256_update(p, n);
    sha256_final(out);
}
