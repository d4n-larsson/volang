#!/bin/sh
# Installs the vo toolchain from the latest GitHub release: one static binary for this machine's
# architecture, checked against the release's SHA256SUMS, written to $VO_INSTALL_DIR (default
# ~/.local/bin). Nothing else is installed or changed.
#
#   curl -fsSL https://raw.githubusercontent.com/d4n-larsson/volang/main/tools/install.sh | sh
#   VO_VERSION=v0.1.0 VO_INSTALL_DIR=/usr/local/bin sh install.sh
set -eu

repo="d4n-larsson/volang"
dir="${VO_INSTALL_DIR:-$HOME/.local/bin}"
version="${VO_VERSION:-latest}"

[ "$(uname -s)" = "Linux" ] || { echo "vo runs on Linux only (this is $(uname -s))." >&2; exit 1; }
case "$(uname -m)" in
    x86_64 | amd64) arch=x86_64 ;;
    aarch64 | arm64) arch=aarch64 ;;
    riscv64) arch=riscv64 ;;
    *) echo "vo has no build for $(uname -m): x86_64, aarch64 and riscv64 only." >&2; exit 1 ;;
esac

if [ "$version" = "latest" ]; then
    base="https://github.com/$repo/releases/latest/download"
else
    base="https://github.com/$repo/releases/download/$version"
fi
file="vo-linux-$arch"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fetch() { # URL FILE
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$1" -o "$2"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "$1" -O "$2"
    else
        echo "install.sh needs curl or wget." >&2
        exit 1
    fi
}
fetch "$base/$file" "$tmp/$file"
fetch "$base/SHA256SUMS" "$tmp/SHA256SUMS"

want=$(grep " $file\$" "$tmp/SHA256SUMS" | cut -d' ' -f1)
got=$(sha256sum "$tmp/$file" | cut -d' ' -f1)
if [ -z "$want" ] || [ "$want" != "$got" ]; then
    echo "The download does not match the release's checksum: nothing was installed." >&2
    exit 1
fi

mkdir -p "$dir"
chmod 755 "$tmp/$file"
mv "$tmp/$file" "$dir/vo"
echo "installed $("$dir/vo" version) to $dir/vo"
case ":$PATH:" in
    *":$dir:"*) ;;
    *) echo "add $dir to your PATH to run it as \`vo\`" ;;
esac
