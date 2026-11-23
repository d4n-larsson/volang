#!/bin/sh
# Vendors the IANA tz database into third_party/tzdata (spec 048 research R1, R2).
#
#   tools/vendor_tzdata.sh VERSION SHA512   fetch tzdataVERSION.tar.gz from data.iana.org, check the
#                                           SHA-512, compile it with `zic -b fat`, pack the zones into
#                                           zones.bin, keep LICENSE and version, write VENDOR.md
#   tools/vendor_tzdata.sh --verify         check the kept files against VENDOR.md
#
# VERSION is the release name (2026f). zones.bin is compiled data, not a file of the release: its
# format is in specs/048-timezones/contracts/timezones.md. `zic` is needed here only, never by make.
# Fat, not slim: the slim output of zic 2.39 is wrong for America/Ojinaga in 2022 (research R1).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=$ROOT/third_party/tzdata
FILES="zones.bin LICENSE version"
SOURCES="africa antarctica asia australasia europe northamerica southamerica etcetera backward"

if [ "${1:-}" = --verify ]; then
    for f in $FILES; do
        want=$(sed -n "s/^- \`$f\` sha256 \([0-9a-f]*\)$/\1/p" "$DEST/VENDOR.md")
        got=$(sha256sum "$DEST/$f" | cut -d' ' -f1)
        [ "$want" = "$got" ] || { echo "vendor_tzdata: $f differs from VENDOR.md" >&2; exit 1; }
    done
    echo "vendor_tzdata: files match VENDOR.md"
    exit 0
fi

VERSION=$1
SHA512=$2
URL=https://data.iana.org/time-zones/releases/tzdata$VERSION.tar.gz
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL -o "$TMP/a.tar.gz" "$URL"
GOT=$(sha512sum "$TMP/a.tar.gz" | cut -d' ' -f1)
[ "$GOT" = "$SHA512" ] || { echo "SHA-512 mismatch for $URL: $GOT" >&2; exit 1; }
mkdir "$TMP/src" "$TMP/out"
tar -xzf "$TMP/a.tar.gz" -C "$TMP/src"
[ "$(cat "$TMP/src/version")" = "$VERSION" ] || { echo "the archive is not release $VERSION" >&2; exit 1; }
# shellcheck disable=SC2086
(cd "$TMP/src" && zic -b fat -d "$TMP/out" $SOURCES)
mkdir -p "$DEST"
cp "$TMP/src/LICENSE" "$TMP/src/version" "$DEST/"
python3 - "$TMP/out" "$DEST/zones.bin" "$VERSION" <<'EOF'
import os, sys
root, out, version = sys.argv[1:]
names = sorted((os.path.relpath(os.path.join(d, f), root).encode() for d, _, fs in os.walk(root) for f in fs))
data, at, index = b"", {}, []
for name in names:
    body = open(os.path.join(root.encode(), name), "rb").read()
    assert body[:4] == b"TZif" and b" " not in name and b"\n" not in name, name
    if body not in at:
        at[body] = len(data)
        data += body
    index.append(b"%s %d %d\n" % (name, at[body], len(body)))
open(out, "wb").write(b"VOTZ1 %s %d\n" % (version.encode(), len(names)) + b"".join(index) + data)
print(f"vendor_tzdata: {len(names)} names, {len(at)} zones, {os.path.getsize(out)} bytes")
EOF
{
    echo "# tzdata $VERSION (vendored)"
    echo
    echo "Pinned by \`tools/vendor_tzdata.sh $VERSION $SHA512\` (spec 048, constitution 1.5.0)."
    echo
    echo "- Source: <$URL>"
    echo "- Archive SHA-512, of the file as downloaded from data.iana.org (IANA publishes a PGP"
    echo "  signature beside it, which this script does not check): \`$SHA512\`"
    echo "- License: public domain. From \`LICENSE\`:"
    echo
    sed -n '1,2p' "$DEST/LICENSE" | sed 's/^/  > /'
    echo
    echo "- Updates: every toolchain release takes the newest tz release (<https://www.iana.org/time-zones>):"
    echo "  run this script with the new version and checksum, \`make test\`, commit this directory."
    echo
    echo "## Kept files"
    echo
    echo "\`LICENSE\` and \`version\` are unmodified files of the archive. \`zones.bin\` is derived (plan,"
    echo "Complexity Tracking): the archive's files \`$SOURCES\`"
    echo "compiled by \`zic -b fat\` ($(zic --version)) and packed, identical zones stored once."
    echo
    for f in $FILES; do echo "- \`$f\` sha256 $(sha256sum "$DEST/$f" | cut -d' ' -f1)"; done
} > "$DEST/VENDOR.md"
echo "vendor_tzdata: tzdata $VERSION in $DEST"
