# Mbed TLS (vendored)

Third-party code built into vo under constitution 1.4.0 ("Implementation Constraints").

| | |
|---|---|
| Library | Mbed TLS 4.1.2 (with TF-PSA-Crypto, as in the release tarball) |
| Source | https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-4.1.2/mbedtls-4.1.2.tar.bz2 |
| SHA-256 | `392215e5e4846df04e2f4c4516079303bd73edde837d8f3e68fb4d4d7ddf0126` |
| License | Apache-2.0 (the project is dual Apache-2.0 / GPL-2.0-or-later; vo uses Apache-2.0), see LICENSE |
| Kept | every header of: include library tf-psa-crypto/include tf-psa-crypto/core tf-psa-crypto/drivers/builtin/include tf-psa-crypto/drivers/builtin/src tf-psa-crypto/platform tf-psa-crypto/utilities tf-psa-crypto/extras tf-psa-crypto/dispatch; the C files below; 158336 lines in all |
| Modified | no (no patches) |

vo's configuration is `src/tls_config.h` (TLS) and `src/tls_crypto_config.h` (cryptography); its
adaptation layer is `src/tls.c`, and the declarations the library needs from a C library are in
`third_party/shim/`.

## Updating

    tools/vendor_mbedtls.sh NEW_VERSION NEW_SHA256

Then review the diff of this directory, adjust the C file list in the script if the build
reports missing or empty objects, and run the full gate. Security releases are applied in the
spec cycle in which they appear.

## C files

- `library/mps_reader.c`
- `library/ssl_ciphersuites.c`
- `library/ssl_client.c`
- `library/ssl_msg.c`
- `library/ssl_tls.c`
- `library/ssl_tls12_client.c`
- `library/ssl_tls13_client.c`
- `library/ssl_tls13_generic.c`
- `library/ssl_tls13_keys.c`
- `library/x509.c`
- `library/x509_crt.c`
- `library/x509_oid.c`
- `tf-psa-crypto/core/psa_crypto.c`
- `tf-psa-crypto/core/psa_crypto_client.c`
- `tf-psa-crypto/core/psa_crypto_driver_wrappers_no_static.c`
- `tf-psa-crypto/core/psa_crypto_random.c`
- `tf-psa-crypto/core/psa_crypto_slot_management.c`
- `tf-psa-crypto/core/psa_util.c`
- `tf-psa-crypto/drivers/builtin/src/aes.c`
- `tf-psa-crypto/drivers/builtin/src/bignum.c`
- `tf-psa-crypto/drivers/builtin/src/bignum_core.c`
- `tf-psa-crypto/drivers/builtin/src/block_cipher.c`
- `tf-psa-crypto/drivers/builtin/src/chacha20.c`
- `tf-psa-crypto/drivers/builtin/src/chachapoly.c`
- `tf-psa-crypto/drivers/builtin/src/ctr_drbg.c`
- `tf-psa-crypto/drivers/builtin/src/ecdsa.c`
- `tf-psa-crypto/drivers/builtin/src/ecp.c`
- `tf-psa-crypto/drivers/builtin/src/ecp_curves.c`
- `tf-psa-crypto/drivers/builtin/src/entropy.c`
- `tf-psa-crypto/drivers/builtin/src/entropy_poll.c`
- `tf-psa-crypto/drivers/builtin/src/gcm.c`
- `tf-psa-crypto/drivers/builtin/src/poly1305.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_aead.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_cipher.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_ecp.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_hash.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_mac.c`
- `tf-psa-crypto/drivers/builtin/src/psa_crypto_rsa.c`
- `tf-psa-crypto/drivers/builtin/src/psa_util_internal.c`
- `tf-psa-crypto/drivers/builtin/src/rsa.c`
- `tf-psa-crypto/drivers/builtin/src/rsa_alt_helpers.c`
- `tf-psa-crypto/drivers/builtin/src/sha1.c`
- `tf-psa-crypto/drivers/builtin/src/sha256.c`
- `tf-psa-crypto/drivers/builtin/src/sha512.c`
- `tf-psa-crypto/extras/md.c`
- `tf-psa-crypto/extras/pk.c`
- `tf-psa-crypto/extras/pk_ecc.c`
- `tf-psa-crypto/extras/pk_rsa.c`
- `tf-psa-crypto/extras/pk_wrap.c`
- `tf-psa-crypto/extras/pkparse.c`
- `tf-psa-crypto/platform/memory_buffer_alloc.c`
- `tf-psa-crypto/platform/platform.c`
- `tf-psa-crypto/platform/platform_util.c`
- `tf-psa-crypto/utilities/asn1parse.c`
- `tf-psa-crypto/utilities/asn1write.c`
- `tf-psa-crypto/utilities/base64.c`
- `tf-psa-crypto/utilities/constant_time.c`
- `tf-psa-crypto/utilities/oid.c`
- `tf-psa-crypto/utilities/pem.c`
