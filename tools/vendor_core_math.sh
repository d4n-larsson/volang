#!/bin/sh
# Vendors CORE-MATH into third_party/core-math (spec 020 research R1, R8; constitution 1.4.1).
#
#   tools/vendor_core_math.sh COMMIT     fetch the kept files at COMMIT, apply patches/, write VENDOR.md
#   tools/vendor_core_math.sh --verify   check the files against VENDOR.md (patches reversed)
#
# CORE-MATH publishes no releases, so it is pinned to a commit, and VENDOR.md records the SHA-256 of
# each kept file as upstream has it (GitLab's generated archives have no stable checksum). The files
# keep their upstream paths: log/dint.h and pow/dint.h differ.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=$ROOT/third_party/core-math
FILES="
LICENSE
src/binary64/acos/acos.c
src/binary64/asin/asin.c
src/binary64/atan/atan.c
src/binary64/atan2/atan2.c
src/binary64/atan2/tint.h
src/binary64/cos/cos.c
src/binary64/exp/exp.c
src/binary64/log/dint.h
src/binary64/log/log.c
src/binary64/pow/dint.h
src/binary64/pow/pow.c
src/binary64/pow/pow.h
src/binary64/pow/qint.h
src/binary64/sin/sin.c
src/binary64/tan/tan.c
"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

if [ "${1:-}" = "--verify" ]; then
    cp -r "$DEST" "$TMP/v"
    for p in $(ls -r "$TMP/v/patches"/*.patch); do patch -s -R -p1 -d "$TMP/v" < "$p"; done
    sed -n 's/^- `\([^`]*\)` \([0-9a-f]\{64\}\)$/\2  \1/p' "$DEST/VENDOR.md" > "$TMP/sums"
    [ "$(wc -l < "$TMP/sums")" -eq "$(echo $FILES | wc -w)" ] || { echo "VENDOR.md lists the wrong number of files" >&2; exit 1; }
    (cd "$TMP/v" && sha256sum --quiet -c "$TMP/sums") || { echo "third_party/core-math differs from VENDOR.md" >&2; exit 1; }
    echo "third_party/core-math matches VENDOR.md"
    exit 0
fi

COMMIT=$1
URL=https://gitlab.inria.fr/core-math/core-math/-/raw/$COMMIT
for f in $FILES; do
    mkdir -p "$TMP/src/$(dirname "$f")"
    curl -sSfL -o "$TMP/src/$f" "$URL/$f"
done
find "$DEST" -mindepth 1 -maxdepth 1 ! -name patches -exec rm -rf {} +
for f in $FILES; do
    mkdir -p "$DEST/$(dirname "$f")"
    cp "$TMP/src/$f" "$DEST/$f"
done
for p in "$DEST/patches"/*.patch; do patch -s -p1 -d "$DEST" < "$p"; done
LINES=$(cd "$TMP/src" && cat $(echo "$FILES" | grep '\.[ch]$') | wc -l)
{
    echo "# CORE-MATH (vendored)"
    echo
    echo "Third-party code built into vo under constitution 1.4.1 (\"Implementation Constraints\")."
    echo
    echo "| | |"
    echo "|---|---|"
    echo "| Library | CORE-MATH, correctly rounded binary64 functions (\`cr_sin\` ... \`cr_pow\`) |"
    echo "| Source | https://gitlab.inria.fr/core-math/core-math |"
    echo "| Commit | \`$COMMIT\` (the project publishes no releases) |"
    echo "| SHA-256 | of each file as upstream has it, below |"
    echo "| License | MIT, see LICENSE |"
    echo "| Kept | the files below, $LINES lines of C |"
    echo "| Modified | one patch, below |"
    echo
    echo "vo calls the functions from \`src/math.c\`, which also supplies the C math functions the compiler"
    echo "emits calls to (\`fma\` on x86-64, \`round\`, ...). Other declarations are in \`third_party/shim/\`."
    echo
    echo "## Files"
    echo
    for f in $FILES; do echo "- \`$f\` $(sha256sum "$TMP/src/$f" | cut -d' ' -f1)"; done
    echo
    echo "## C files"
    echo
    for f in $FILES; do case $f in *.c) echo "- \`$f\`";; esac; done
    echo
    echo "## Patches"
    echo
    for p in "$DEST/patches"/*.patch; do echo "- \`patches/$(basename "$p")\`: $(head -1 "$p")"; done
    echo
    echo "## Updating"
    echo
    echo "    tools/vendor_core_math.sh NEW_COMMIT"
    echo "    tools/vendor_core_math.sh --verify"
    echo
    echo "Then review the diff of this directory, check that the patches still apply and are still needed,"
    echo "and run the full gate (the vectors in \`tests/math/\` must still match bit for bit). A fix for a"
    echo "wrong result is applied in the spec cycle in which it appears."
} > "$DEST/VENDOR.md"
echo "vendored CORE-MATH $COMMIT into third_party/core-math ($LINES lines)"
