/* Spec 019 (FR-012, FR-013, research R11): the vendored cryptography against published vectors.
 *
 * Built by `make tls-selftest` from the same vendored sources and configuration as vo (plus
 * MBEDTLS_SELF_TEST), as a static test program for each platform (the C library here only prints),
 * and run natively and under qemu: every line must say "ok" and the output must be identical on
 * x86-64, ARM64 and RISC-V. The library's own self-tests use NIST and RFC vectors; HMAC (RFC 4231),
 * HKDF (RFC 5869) and X25519 (RFC 7748) are checked here through the PSA interface vo uses. */
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

#include "mbedtls/private/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/private/bignum.h"
#include "mbedtls/private/chacha20.h"
#include "mbedtls/private/chachapoly.h"
#include "mbedtls/private/ecp.h"
#include "mbedtls/private/gcm.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/poly1305.h"
#include "mbedtls/private/rsa.h"
#include "mbedtls/private/sha256.h"
#include "mbedtls/private/sha512.h"
#include "psa/crypto.h"

int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags, size_t *bits, unsigned char *out, size_t n)
{
    (void)flags;
    size_t got = 0;
    while (got < n) {
        ssize_t r = getrandom(out + got, n - got, 0);
        if (r <= 0) return PSA_ERROR_INSUFFICIENT_ENTROPY;
        got += (size_t)r;
    }
    *bits = 8 * n;
    return 0;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return 0;
}

struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *buf)
{
    time_t t = (time_t)*tt;
    return gmtime_r(&t, buf);
}

static int fails;

static void report(const char *name, int ret)
{
    printf("%-28s %s\n", name, ret == 0 ? "ok" : "FAILED");
    fails += ret != 0;
}

static int hex(const char *h, unsigned char *out)
{
    int n = 0;
    for (; h[0] && h[1]; h += 2) {
        unsigned v;
        sscanf(h, "%2x", &v);
        out[n++] = (unsigned char)v;
    }
    return n;
}

static int same(const unsigned char *a, const char *want_hex)
{
    unsigned char w[128];
    int n = hex(want_hex, w);
    return memcmp(a, w, (size_t)n) == 0 ? 0 : 1;
}

/* RFC 4231 test case 2: HMAC-SHA-256, key "Jefe". */
static int hmac_kat(void)
{
    psa_key_attributes_t at = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&at, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&at, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&at, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_key_id_t k;
    if (psa_import_key(&at, (const uint8_t *)"Jefe", 4, &k)) return 1;
    uint8_t mac[32];
    size_t n;
    const char *msg = "what do ya want for nothing?";
    int r = psa_mac_compute(k, PSA_ALG_HMAC(PSA_ALG_SHA_256), (const uint8_t *)msg, strlen(msg), mac, 32, &n);
    psa_destroy_key(k);
    return r || same(mac, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

/* RFC 5869 test case 1: HKDF-SHA-256. */
static int hkdf_kat(void)
{
    unsigned char ikm[22], salt[13], info[10], okm[42];
    int ni = hex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", ikm);
    int ns = hex("000102030405060708090a0b0c", salt);
    int nf = hex("f0f1f2f3f4f5f6f7f8f9", info);
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    int r = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256)) ||
            psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, (size_t)ns) ||
            psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm, (size_t)ni) ||
            psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, info, (size_t)nf) ||
            psa_key_derivation_output_bytes(&op, okm, 42);
    psa_key_derivation_abort(&op);
    return r || same(okm, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
}

/* RFC 7748 section 5.2, the first X25519 vector. */
static int x25519_kat(void)
{
    unsigned char scalar[32], u[32];
    hex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", scalar);
    hex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", u);
    psa_key_attributes_t at = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&at, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&at, 255);
    psa_set_key_usage_flags(&at, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&at, PSA_ALG_ECDH);
    psa_key_id_t k;
    if (psa_import_key(&at, scalar, 32, &k)) return 1;
    uint8_t out[32];
    size_t n;
    int r = psa_raw_key_agreement(PSA_ALG_ECDH, k, u, 32, out, 32, &n);
    psa_destroy_key(k);
    return r || same(out, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");
}

int main(void)
{
    static unsigned char arena[1 << 20];
    mbedtls_memory_buffer_alloc_init(arena, sizeof arena);
    report("aes", mbedtls_aes_self_test(0));
    report("gcm", mbedtls_gcm_self_test(0));
    report("chacha20", mbedtls_chacha20_self_test(0));
    report("poly1305", mbedtls_poly1305_self_test(0));
    report("chacha20-poly1305", mbedtls_chachapoly_self_test(0));
    report("sha256", mbedtls_sha256_self_test(0));
    report("sha384", mbedtls_sha384_self_test(0));
    report("sha512", mbedtls_sha512_self_test(0));
    report("bignum", mbedtls_mpi_self_test(0));
    report("ecp", mbedtls_ecp_self_test(0));
    report("rsa", mbedtls_rsa_self_test(0));
    report("base64", mbedtls_base64_self_test(0));
    report("psa init", psa_crypto_init() != PSA_SUCCESS);
    report("hmac-sha256 (rfc 4231)", hmac_kat());
    report("hkdf-sha256 (rfc 5869)", hkdf_kat());
    report("x25519 (rfc 7748)", x25519_kat());
    report("memory buffer allocator", mbedtls_memory_buffer_alloc_self_test(0)); /* last: it resets the allocator */
    printf("%s\n", fails ? "SOME FAILED" : "all passed");
    return fails != 0;
}
