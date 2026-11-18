/* vo's TF-PSA-Crypto configuration (spec 019 research R2, R4): what a TLS client verifying
 * certificates needs, built freestanding (no files, no assembly, vo's own memory, entropy, time). */
#define TF_PSA_CRYPTO_CONFIG_VERSION 0x01000000
#define PSA_WANT_ALG_CHACHA20_POLY1305 1
#define PSA_WANT_ALG_ECDH 1
#define PSA_WANT_ALG_ECDSA 1
#define PSA_WANT_ALG_GCM 1
#define PSA_WANT_ALG_HKDF 1
#define PSA_WANT_ALG_HKDF_EXTRACT 1
#define PSA_WANT_ALG_HKDF_EXPAND 1
#define PSA_WANT_ALG_HMAC 1
#define PSA_WANT_ALG_RSA_PKCS1V15_SIGN 1
#define PSA_WANT_ALG_RSA_PSS 1
#define PSA_WANT_ALG_SHA_1 1 /* only so that roots self-signed with SHA-1 parse: the verification profile rejects SHA-1 in chains */
#define PSA_WANT_ALG_SHA_256 1
#define PSA_WANT_ALG_SHA_384 1
#define PSA_WANT_ALG_SHA_512 1
#define PSA_WANT_ALG_TLS12_PRF 1
#define PSA_WANT_ECC_MONTGOMERY_255 1
#define PSA_WANT_ECC_SECP_R1_256 1
#define PSA_WANT_ECC_SECP_R1_384 1
#define PSA_WANT_ECC_SECP_R1_521 1 /* one root of the common trust stores has a P-521 key */
#define PSA_WANT_KEY_TYPE_DERIVE 1
#define PSA_WANT_KEY_TYPE_HMAC 1
#define PSA_WANT_KEY_TYPE_AES 1
#define PSA_WANT_KEY_TYPE_CHACHA20 1
#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY 1
#define PSA_WANT_KEY_TYPE_RAW_DATA 1
#define PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_IMPORT 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_EXPORT 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
/* Blocks aligned for 64-bit platforms (the default, 4, gives misaligned headers: found by the
 * instrumented build) */
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE 8
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#if !defined(VO_TLS_SELF_TEST) /* (the self-test program prints with its C library) */
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#endif
#define MBEDTLS_PLATFORM_TIME_ALT
#define MBEDTLS_PLATFORM_GMTIME_R_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_PLATFORM_SNPRINTF_ALT
#define MBEDTLS_PLATFORM_VSNPRINTF_ALT
#define MBEDTLS_MD_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_DRIVER_GET_ENTROPY
#define MBEDTLS_PSA_KEY_STORE_DYNAMIC
#define MBEDTLS_ECP_NIST_OPTIM
#define PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC 1
/* The same C on every platform (constitution: no assembly, identical results): no NEON variants.
 * The Makefile also builds the library with -U__ARM_NEON. */
#define MBEDTLS_CHACHA20_NEON_MULTIBLOCK 0
/* tests/tls/selftest.c (make tls-selftest) builds the library again with its self-tests. */
#if defined(VO_TLS_SELF_TEST)
#define MBEDTLS_SELF_TEST
#endif
/* Software GCM with 8-bit tables (about 4x the 4-bit default): no AES or carry-less multiply
 * instructions are used, so this is what keeps HTTPS downloads near plain HTTP speed (SC-004). */
#define MBEDTLS_GCM_LARGE_TABLE
