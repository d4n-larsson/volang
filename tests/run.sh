#!/bin/sh
# Golden-case runner. Conventions: specs/001-vo-lang-core/tasks.md "Golden-case conventions".
# Usage: tests/run.sh [--repeat N] [--accept] [PATH...]
# --accept writes the actual stdout/stderr/exit status of failing non-fmt cases as their
# expectations (review the diff first: `git diff tests/cases`).
root=$(cd "$(dirname "$0")/.." && pwd)
vo="$root/bin/vo"
repeat=1
accept=0
if [ "$1" = --repeat ]; then repeat=$2; shift 2; fi
if [ "$1" = --accept ]; then accept=1; shift; fi
[ $# -eq 0 ] && set -- "$root/tests/cases"

tmp="$root/build/tmp"
work="$root/build/run-work"
mkdir -p "$work"
passed=0; failed=0

normalize() { sed -E 's/"ms":[0-9]+/"ms":0/g; s/\(([0-9]+)ms\)/(0ms)/g'; }

# area <case path>: the directory right below tests/cases, or the parent directory name.
area() {
    case "$1" in
        "$root"/tests/cases/*) rel=${1#"$root"/tests/cases/}; echo "${rel%%/*}" ;;
        *) basename "$(dirname "$1")" ;;
    esac
}

# run_once <base> <out-prefix>: runs one case, writes <prefix>.out/.err/.code (and .file for fmt).
run_once() {
    base=$1; pre=$2; dir=$(cd "$(dirname "$base")" && pwd); file="$dir/$(basename "$base").vo"
    rm -rf "$tmp"; mkdir -p "$tmp"
    if [ -f "$base.cmd" ]; then
        cmd=$(sed -e "s|{dir}|$dir|g" -e "s|{}|$file|g" "$base.cmd")
    else
        case $(area "$base") in
            lex|parse|check) cmd="check --json $file" ;;
            run|multi|effects|func|generic) cmd="run $file" ;;
            test) cmd="test --json $file" ;;
            fmt) cmd=FMT ;;
            *) cmd="run $file" ;;
        esac
    fi
    if [ "$cmd" = FMT ]; then
        cp "$file" "$tmp/case.vo"
        (cd "$tmp" && timeout 60 "$vo" fmt "$tmp/case.vo" >"$pre.out" 2>"$pre.err"; echo $? >"$pre.code")
        cp "$tmp/case.vo" "$pre.file"
        (cd "$tmp" && timeout 60 "$vo" fmt --check "$tmp/case.vo" >/dev/null 2>&1; echo $? >"$pre.check")
        return
    fi
    if [ -f "$base.in" ]; then input="$base.in"; else input=/dev/null; fi
    # shellcheck disable=SC2086
    if [ -f "$base.hold-stdin" ]; then
        (cd "$tmp" && tail -f /dev/null | timeout 60 "$vo" $cmd >"$pre.out.raw" 2>"$pre.err.raw"; echo $? >"$pre.code")
    else
        (cd "$tmp" && timeout 60 "$vo" $cmd <"$input" >"$pre.out.raw" 2>"$pre.err.raw"; echo $? >"$pre.code")
    fi
    normalize <"$pre.out.raw" | sed "s|$dir/||g" >"$pre.out"; normalize <"$pre.err.raw" | sed "s|$dir/||g" >"$pre.err"
    rm -f "$pre.out.raw" "$pre.err.raw"
}

check_case() {
    base=$1; name=${base#"$root"/}; pre="$work/actual"; ok=1; report=""
    run_once "$base" "$pre"
    if [ -f "$pre.file" ]; then
        if ! diff -u "$base.expected.vo" "$pre.file" >"$work/d" 2>&1; then ok=0; report="$report$(cat "$work/d")
"; fi
        if [ "$(cat "$pre.check")" != 0 ]; then ok=0; report="${report}fmt --check after fmt exited $(cat "$pre.check")
"; fi
        rm -f "$pre.file" "$pre.check"
    else
        for s in out err; do
            [ "$s" = out ] && [ -f "$base.skip-out" ] && continue
            if [ -f "$base.$s" ]; then exp="$base.$s"; else exp=/dev/null; fi
            if ! diff -u "$exp" "$pre.$s" >"$work/d" 2>&1; then ok=0; report="$report--- std$s:
$(cat "$work/d")
"; fi
        done
    fi
    if [ -f "$base.code" ]; then want=$(tr -d ' \n' <"$base.code"); else want=0; fi
    got=$(cat "$pre.code")
    [ "$got" != "$want" ] && { ok=0; report="${report}exit status $got, expected $want
"; }
    i=1
    while [ $ok = 1 ] && [ "$i" -lt "$repeat" ]; do
        cp "$pre.out" "$work/first.out"; cp "$pre.err" "$work/first.err"; cp "$pre.code" "$work/first.code"
        run_once "$base" "$work/again"
        for s in out err code; do
            cmp -s "$work/first.$s" "$work/again.$s" || { ok=0; report="${report}run $((i+1)) differs in $s
"; }
        done
        i=$((i+1))
    done
    if [ $ok = 1 ]; then echo "PASS $name"; passed=$((passed+1))
    elif [ $accept = 1 ] && [ ! -f "$base.expected.vo" ]; then
        for s in out err; do
            [ "$s" = out ] && [ -f "$base.skip-out" ] && continue
            if [ -s "$pre.$s" ]; then cp "$pre.$s" "$base.$s"; else rm -f "$base.$s"; fi
        done
        cp "$pre.code" "$base.code"
        echo "ACCEPT $name"; passed=$((passed+1))
    else echo "FAIL $name"; printf '%s' "$report" | sed 's/^/    /'; failed=$((failed+1)); fi
}

cases=$(for p in "$@"; do
    p=$(cd "$(dirname "$p")" && echo "$(pwd)/$(basename "$p")")
    if [ -d "$p" ]; then find "$p" \( -name '*.vo' -o -name '*.cmd' \) ; else echo "$p"; fi
done | sed -E 's/\.(vo|cmd)$//' | sort -u)

for base in $cases; do
    case $base in *.expected) continue ;; esac
    if [ -f "$base.vo" ]; then
        [ -f "$base.cmd" ] || [ -f "$base.out" ] || [ -f "$base.err" ] || [ -f "$base.code" ] || continue
    elif [ ! -f "$base.cmd" ]; then continue; fi
    check_case "$base"
done
echo "passed $passed, failed $failed"
[ "$failed" = 0 ]
