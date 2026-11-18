#!/bin/sh
# Per-edit check for C sources (spec 008 FR-012, constitution 1.4.0): a Claude Code PostToolUse hook
# beside the SESI hook. Reads the hook's JSON on stdin; when the edited file is src/*.c or src/vo.h,
# compiles it (every C file, for the header) with the toolchain's flags and strict warnings, and
# reports problems with exit status 2 (shown to the editor). Other files pass through.
root=$(cd "$(dirname "$0")/.." && pwd)
file=$(python3 -c 'import json,sys; d=json.load(sys.stdin); print(d.get("tool_input",{}).get("file_path",""))' 2>/dev/null)
case "$file" in
    "$root"/src/*.c) files=$file ;;
    "$root"/src/vo.h|"$root"/src/platform/*.h) files=$(ls "$root"/src/*.c 2>/dev/null) ;;
    *) exit 0 ;;
esac
[ -n "$files" ] || exit 0
cd "$root" || exit 0
flags=$(make -s -f Makefile -f - print-cflags <<'MK' 2>/dev/null
print-cflags:
	@echo $(CC) $(CFLAGS) $(CWARN) $(TLS_FLAGS)
MK
)
[ -n "$flags" ] || { echo "cc_check: could not read the compiler flags from the Makefile" >&2; exit 2; }
out=""
for f in $files; do
    o=$($flags -I src -fsyntax-only "$f" 2>&1) || out="$out$o
"
done
if [ -n "$out" ]; then
    printf 'C checks found problems:\n%s' "$out" >&2
    exit 2
fi
exit 0
