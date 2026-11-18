#!/bin/sh
# Vendors Mbed TLS into third_party/mbedtls (spec 019 research R1, R13; constitution 1.4.0).
#
#   tools/vendor_mbedtls.sh VERSION SHA256
#
# Downloads the release tarball, checks its SHA-256, and extracts only the files vo builds: every
# header of the kept directories and the C files listed below (the ones that hold code under vo's
# configuration, src/tls_config.h and src/tls_crypto_config.h). The files are kept unmodified.
# Then it writes third_party/mbedtls/VENDOR.md. An update is: run this with the new version, review
# the diff, run the full gate.
set -eu
VERSION=$1
SHA=$2
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=$ROOT/third_party/mbedtls
URL=https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$VERSION/mbedtls-$VERSION.tar.bz2
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -sSfL -o "$TMP/m.tar.bz2" "$URL"
echo "$SHA  $TMP/m.tar.bz2" | sha256sum -c - >/dev/null || { echo "checksum mismatch for $URL" >&2; exit 1; }
tar -xjf "$TMP/m.tar.bz2" -C "$TMP"
SRC=$TMP/mbedtls-$VERSION
C_FILES="
library/mps_reader.c
library/ssl_ciphersuites.c
library/ssl_client.c
library/ssl_msg.c
library/ssl_tls.c
library/ssl_tls12_client.c
library/ssl_tls13_client.c
library/ssl_tls13_generic.c
library/ssl_tls13_keys.c
library/x509.c
library/x509_crt.c
library/x509_oid.c
tf-psa-crypto/core/psa_crypto.c
tf-psa-crypto/core/psa_crypto_client.c
tf-psa-crypto/core/psa_crypto_driver_wrappers_no_static.c
tf-psa-crypto/core/psa_crypto_random.c
tf-psa-crypto/core/psa_crypto_slot_management.c
tf-psa-crypto/core/psa_util.c
tf-psa-crypto/drivers/builtin/src/aes.c
tf-psa-crypto/drivers/builtin/src/bignum.c
tf-psa-crypto/drivers/builtin/src/bignum_core.c
tf-psa-crypto/drivers/builtin/src/block_cipher.c
tf-psa-crypto/drivers/builtin/src/chacha20.c
tf-psa-crypto/drivers/builtin/src/chachapoly.c
tf-psa-crypto/drivers/builtin/src/ctr_drbg.c
tf-psa-crypto/drivers/builtin/src/ecdsa.c
tf-psa-crypto/drivers/builtin/src/ecp.c
tf-psa-crypto/drivers/builtin/src/ecp_curves.c
tf-psa-crypto/drivers/builtin/src/entropy.c
tf-psa-crypto/drivers/builtin/src/entropy_poll.c
tf-psa-crypto/drivers/builtin/src/gcm.c
tf-psa-crypto/drivers/builtin/src/poly1305.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_aead.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_cipher.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_ecp.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_hash.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_mac.c
tf-psa-crypto/drivers/builtin/src/psa_crypto_rsa.c
tf-psa-crypto/drivers/builtin/src/psa_util_internal.c
tf-psa-crypto/drivers/builtin/src/rsa.c
tf-psa-crypto/drivers/builtin/src/rsa_alt_helpers.c
tf-psa-crypto/drivers/builtin/src/sha1.c
tf-psa-crypto/drivers/builtin/src/sha256.c
tf-psa-crypto/drivers/builtin/src/sha512.c
tf-psa-crypto/extras/md.c
tf-psa-crypto/extras/pk.c
tf-psa-crypto/extras/pk_ecc.c
tf-psa-crypto/extras/pk_rsa.c
tf-psa-crypto/extras/pk_wrap.c
tf-psa-crypto/extras/pkparse.c
tf-psa-crypto/platform/memory_buffer_alloc.c
tf-psa-crypto/platform/platform.c
tf-psa-crypto/platform/platform_util.c
tf-psa-crypto/utilities/asn1parse.c
tf-psa-crypto/utilities/asn1write.c
tf-psa-crypto/utilities/base64.c
tf-psa-crypto/utilities/constant_time.c
tf-psa-crypto/utilities/oid.c
tf-psa-crypto/utilities/pem.c
"
HEADER_DIRS="include library tf-psa-crypto/include tf-psa-crypto/core tf-psa-crypto/drivers/builtin/include
tf-psa-crypto/drivers/builtin/src tf-psa-crypto/platform tf-psa-crypto/utilities tf-psa-crypto/extras
tf-psa-crypto/dispatch"
rm -rf "$DEST"
mkdir -p "$DEST"
cp "$SRC/LICENSE" "$DEST/LICENSE"
for d in $HEADER_DIRS; do
    (cd "$SRC" && find "$d" -name '*.h') | while read -r h; do
        mkdir -p "$DEST/$(dirname "$h")"
        cp "$SRC/$h" "$DEST/$h"
    done
done
for c in $C_FILES; do
    mkdir -p "$DEST/$(dirname "$c")"
    cp "$SRC/$c" "$DEST/$c"
done
LINES=$(cd "$DEST" && find . -name '*.[ch]' -exec cat {} + | wc -l)
{
    echo "# Mbed TLS (vendored)"
    echo
    echo "Third-party code built into vo under constitution 1.4.0 (\"Implementation Constraints\")."
    echo
    echo "| | |"
    echo "|---|---|"
    echo "| Library | Mbed TLS $VERSION (with TF-PSA-Crypto, as in the release tarball) |"
    echo "| Source | $URL |"
    echo "| SHA-256 | \`$SHA\` |"
    echo "| License | Apache-2.0 (the project is dual Apache-2.0 / GPL-2.0-or-later; vo uses Apache-2.0), see LICENSE |"
    echo "| Kept | every header of: $(echo $HEADER_DIRS); the C files below; $LINES lines in all |"
    echo "| Modified | no (no patches) |"
    echo
    echo "vo's configuration is \`src/tls_config.h\` (TLS) and \`src/tls_crypto_config.h\` (cryptography); its"
    echo "adaptation layer is \`src/tls.c\`, and the declarations the library needs from a C library are in"
    echo "\`third_party/shim/\`."
    echo
    echo "## Updating"
    echo
    echo "    tools/vendor_mbedtls.sh NEW_VERSION NEW_SHA256"
    echo
    echo "Then review the diff of this directory, adjust the C file list in the script if the build"
    echo "reports missing or empty objects, and run the full gate. Security releases are applied in the"
    echo "spec cycle in which they appear."
    echo
    echo "## C files"
    echo
    for c in $C_FILES; do echo "- \`$c\`"; done
} > "$DEST/VENDOR.md"
echo "vendored Mbed TLS $VERSION into third_party/mbedtls ($LINES lines)"
