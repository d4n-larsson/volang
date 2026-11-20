/* Spec 035: what std.crypto builds on SHA-256 (src/sha256.c): HMAC (RFC 2104), PBKDF2 (RFC 8018) and a
 * comparison whose time does not depend on where the inputs differ. */
#include "vo.h"

/* Hashes the key's two pads once; every message for this key starts from them. */
void hmac_key(Hmac *h, char *key, long klen)
{
    char k[64] = {0}, pad[64];
    if (klen > 64)
        sha256(key, klen, k); /* a long key is used as its hash */
    else
        for (long i = 0; i < klen; i++) k[i] = key[i];
    for (int i = 0; i < 64; i++) pad[i] = (char)(k[i] ^ 0x36);
    sha256_start(&h->inner);
    sha256_add(&h->inner, pad, 64);
    for (int i = 0; i < 64; i++) pad[i] = (char)(k[i] ^ 0x5c);
    sha256_start(&h->outer);
    sha256_add(&h->outer, pad, 64);
}

void hmac_sha256(Hmac *h, char *msg, long mlen, char *out)
{
    Sha256 c = h->inner;
    char inner[32];
    sha256_add(&c, msg, mlen);
    sha256_end(&c, inner);
    c = h->outer;
    sha256_add(&c, inner, 32);
    sha256_end(&c, out);
}

/* The first 32-byte block of PBKDF2-HMAC-SHA256: U1 = HMAC(pw, salt || 00000001), Un = HMAC(pw, Un-1),
 * out = U1 ^ ... ^ Un. After U1 every message is 32 bytes on top of a 64-byte pad, so each HMAC is two
 * compressions of a block laid out once: 32 bytes, 0x80, zeros, the length 768 bits. */
void pbkdf2_sha256(char *pw, long pwlen, char *salt, long saltlen, long iterations, char *out)
{
    Hmac h;
    hmac_key(&h, pw, pwlen);
    Sha256 c = h.inner;
    char u[32], one[4] = {0, 0, 0, 1};
    sha256_add(&c, salt, saltlen);
    sha256_add(&c, one, 4);
    sha256_end(&c, u);
    c = h.outer;
    sha256_add(&c, u, 32);
    sha256_end(&c, u);
    unsigned char block[64] = {0};
    uint32_t acc[8], st[8];
    for (int i = 0; i < 32; i++) block[i] = (unsigned char)u[i];
    block[32] = 0x80;
    block[62] = 0x03; /* (64 + 32) * 8 = 768 bits */
    for (int i = 0; i < 8; i++)
        acc[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 | (uint32_t)block[4 * i + 2] << 8 |
                 block[4 * i + 3];
    for (long n = 1; n < iterations; n++) {
        if (!(n & 1023)) POLL(16384); /* --timeout applies inside a hash */
        for (int pass = 0; pass < 2; pass++) { /* inner, then outer, each from its saved pad state */
            for (int i = 0; i < 8; i++) st[i] = pass ? h.outer.h[i] : h.inner.h[i];
            sha256_compress(st, block);
            for (int i = 0; i < 8; i++) {
                block[4 * i] = (unsigned char)(st[i] >> 24);
                block[4 * i + 1] = (unsigned char)(st[i] >> 16);
                block[4 * i + 2] = (unsigned char)(st[i] >> 8);
                block[4 * i + 3] = (unsigned char)st[i];
            }
        }
        for (int i = 0; i < 8; i++) acc[i] ^= st[i];
    }
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (char)(acc[i] >> 24);
        out[4 * i + 1] = (char)(acc[i] >> 16);
        out[4 * i + 2] = (char)(acc[i] >> 8);
        out[4 * i + 3] = (char)acc[i];
    }
}

/* a == b without stopping at the first difference (the lengths are not secret). */
int bytes_same(char *a, long alen, char *b, long blen)
{
    if (alen != blen) return 0;
    volatile unsigned char diff = 0; /* volatile: the loop must not be cut short by the compiler */
    for (long i = 0; i < alen; i++) diff = (unsigned char)(diff | (a[i] ^ b[i]));
    return diff == 0;
}

/* ---- the builtins of std.crypto ---- */

static Str *digest_str(char *d)
{
    return str_new(d, 32);
}

Str *crypto_sha256(Str *b)
{
    char d[32];
    sha256(b->data, b->len, d);
    return digest_str(d);
}

Str *crypto_hmac(Str *key, Str *msg)
{
    Hmac h;
    char d[32];
    hmac_key(&h, key->data, key->len);
    hmac_sha256(&h, msg->data, msg->len, d);
    return digest_str(d);
}

/* R0007 for a cost outside 1 to 10,000,000 (std.crypto checks stored texts before it calls). */
Str *crypto_pbkdf2(Str *pw, Str *salt, long iterations, Node *call)
{
    if (iterations < 1 || iterations > 10000000)
        rt_raise("R0007", msg("A password hash needs 1 to 10000000 iterations, got $1.", int_to_str(iterations), 0, 0),
                 std_user_call(call));
    char d[32];
    GC_ROOT(&pw); /* (POLL may collect) */
    GC_ROOT(&salt);
    pbkdf2_sha256(pw->data, pw->len, salt->data, salt->len, iterations, d);
    GC_UNROOT(2);
    return digest_str(d);
}
