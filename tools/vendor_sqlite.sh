#!/bin/sh
# Vendors the SQLite amalgamation into third_party/sqlite (spec 026 research R1; constitution 1.5.0).
#
#   tools/vendor_sqlite.sh VERSION YEAR SHA3   fetch sqlite-amalgamation-VERSION.zip from sqlite.org/YEAR/,
#                                              check SQLite's published SHA3-256, keep sqlite3.c and
#                                              sqlite3.h, write VENDOR.md
#   tools/vendor_sqlite.sh --verify            check the kept files against VENDOR.md
#
# VERSION is SQLite's 7-digit form (3540100 for 3.54.1); YEAR and SHA3 are on sqlite.org/download.html.
# The files are kept unmodified; configuration is compile options (VENDOR.md, Makefile) and the VFS.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=$ROOT/third_party/sqlite
FILES="sqlite3.c sqlite3.h"

if [ "${1:-}" = --verify ]; then
    for f in $FILES; do
        want=$(sed -n "s/^- \`$f\` sha256 \([0-9a-f]*\)$/\1/p" "$DEST/VENDOR.md")
        got=$(sha256sum "$DEST/$f" | cut -d' ' -f1)
        [ "$want" = "$got" ] || { echo "vendor_sqlite: $f differs from VENDOR.md" >&2; exit 1; }
    done
    echo "vendor_sqlite: files match VENDOR.md"
    exit 0
fi

VERSION=$1
YEAR=$2
SHA3=$3
URL=https://sqlite.org/$YEAR/sqlite-amalgamation-$VERSION.zip
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL -o "$TMP/a.zip" "$URL"
GOT=$(python3 -c "import hashlib,sys; print(hashlib.sha3_256(open(sys.argv[1],'rb').read()).hexdigest())" "$TMP/a.zip")
[ "$GOT" = "$SHA3" ] || { echo "SHA3-256 mismatch for $URL: $GOT" >&2; exit 1; }
python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" "$TMP/a.zip" "$TMP"
SRC=$TMP/sqlite-amalgamation-$VERSION
mkdir -p "$DEST"
for f in $FILES; do cp "$SRC/$f" "$DEST/$f"; done
DOTTED=$(echo "$VERSION" | sed 's/^\(.\)\(..\)\(..\)\(..\)$/\1.\2.\3.\4/' | sed 's/\.0\([0-9]\)/.\1/g; s/\.0$//')

{
    echo "# SQLite $DOTTED (vendored)"
    echo
    echo "Pinned by \`tools/vendor_sqlite.sh $VERSION $YEAR $SHA3\` (spec 026, constitution 1.5.0)."
    echo
    echo "- Source: <$URL>"
    echo "- Archive SHA3-256 (as published on sqlite.org/download.html): \`$SHA3\`"
    echo "- License: public domain. From the top of \`sqlite3.c\`:"
    echo
    L=$(grep -n 'The author disclaims copyright' "$DEST/sqlite3.c" | head -1 | cut -d: -f1)
    sed -n "${L},$((L + 5))p" "$DEST/sqlite3.c" | sed 's/^/  > /'
    echo
    echo "- Security releases: <https://sqlite.org/cves.html>; a release that fixes a vulnerability is applied"
    echo "  in the spec cycle in which it appears (constitution, Implementation Constraints)."
    echo
    echo "## Kept files (unmodified)"
    echo
    for f in $FILES; do echo "- \`$f\` sha256 $(sha256sum "$DEST/$f" | cut -d' ' -f1)"; done
    echo
    echo "## Compile options"
    echo
    echo "See \`SQLITE_FLAGS\` in the Makefile and research R2 of specs/026-sqlite/research.md."
} > "$DEST/VENDOR.md"
echo "vendor_sqlite: SQLite $DOTTED in $DEST"
