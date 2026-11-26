#!/bin/sh
# Golden-case runner. Conventions: specs/001-vo-lang-core/tasks.md "Golden-case conventions".
# Usage: tests/run.sh [--repeat N] [--stress [N]] [--accept] [PATH...]
# --stress N also runs every case with the stress build (build-stress/vo, or $VO_STRESS_BIN) collecting
# every Nth allocation (default 64), and requires byte-identical stdout, stderr and exit status.
# A case with a large live heap can name a larger interval in <case>.stress-every (the larger wins);
# `skip` there leaves out cases whose output legitimately depends on collections or on time
# (memory reports, timeouts).
# --accept writes the actual stdout/stderr/exit status of failing non-fmt cases as their
# expectations (review the diff first: `git diff tests/cases`).
# Cases run in $VO_JOBS parallel workers (default: the number of cores), each in its own
# build/tmp.K and build/run-work.K; VO_JOBS=1 runs them one at a time, in order.
root=$(cd "$(dirname "$0")/.." && pwd)
vo="$root/bin/vo"
repeat=1
accept=0
stress=0
if [ "$1" = --repeat ]; then repeat=$2; shift 2; fi
if [ "$1" = --stress ]; then
    shift; stress=64
    case $1 in ''|*[!0-9]*) ;; *) stress=$1; shift ;; esac
fi
stress_bin=${VO_STRESS_BIN:-$root/build-stress/vo}
if [ "$1" = --accept ]; then accept=1; shift; fi
[ $# -eq 0 ] && set -- "$root/tests/cases"

tmp="$root/build/tmp"
work="$root/build/run-work"
mkdir -p "$work"
passed=0; failed=0

normalize() { sed -E 's/"ms":[0-9]+/"ms":0/g; s/([ (])[0-9]+ms\)/\10ms)/g'; }

# area <case path>: the directory right below tests/cases, or the parent directory name.
area() {
    case "$1" in
        "$root"/tests/cases/*) rel=${1#"$root"/tests/cases/}; echo "${rel%%/*}" ;;
        *) basename "$(dirname "$1")" ;;
    esac
}

# run_once <base> <out-prefix>: runs one case, writes <prefix>.out/.err/.code (and .file for fmt).
# It must not touch check_case's $pre: the stress and repeat runs compare against it.
run_once() {
    base=$1; rpre=$2; dir=$(cd "$(dirname "$base")" && pwd); file="$dir/$(basename "$base").vo"
    rm -rf "$tmp"; mkdir -p "$tmp"
    if [ -f "$base.cmd" ]; then
        cmd=$(sed -e "s|{dir}|$dir|g" -e "s|{}|$file|g" "$base.cmd")
    else
        case $(area "$base") in
            lex|parse|check|library) cmd="check --json $file" ;;
            run|multi|effects|func|generic) cmd="run $file" ;;
            test) cmd="test --json $file" ;;
            fmt) cmd=FMT ;;
            *) cmd="run $file" ;;
        esac
    fi
    if [ "$cmd" = FMT ]; then
        cp "$file" "$tmp/case.vo"
        (cd "$tmp" && timeout 60 "$vo" fmt "$tmp/case.vo" >"$rpre.out" 2>"$rpre.err"; echo $? >"$rpre.code")
        cp "$tmp/case.vo" "$rpre.file"
        (cd "$tmp" && timeout 60 "$vo" fmt --check "$tmp/case.vo" >/dev/null 2>&1; echo $? >"$rpre.check")
        return
    fi
    if [ -f "$base.in" ]; then input="$base.in"; else input=/dev/null; fi
    # shellcheck disable=SC2086
    if [ -f "$base.hold-stdin" ]; then
        (cd "$tmp" && tail -f /dev/null | timeout 60 "$vo" $cmd >"$rpre.out.raw" 2>"$rpre.err.raw"; echo $? >"$rpre.code")
    else
        (cd "$tmp" && timeout 60 "$vo" $cmd <"$input" >"$rpre.out.raw" 2>"$rpre.err.raw"; echo $? >"$rpre.code")
    fi
    normalize <"$rpre.out.raw" | sed "s|$dir/||g" >"$rpre.out"; normalize <"$rpre.err.raw" | sed "s|$dir/||g" >"$rpre.err"
    rm -f "$rpre.out.raw" "$rpre.err.raw"
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
    if [ $ok = 1 ] && [ "$stress" != 0 ] && [ ! -f "$pre.file" ] && [ -f "$pre.out" ] \
        && [ "$(cat "$base.stress-every" 2>/dev/null)" != skip ]; then
        every=$stress
        [ -f "$base.stress-every" ] && [ "$(cat "$base.stress-every")" -gt "$every" ] && every=$(cat "$base.stress-every")
        VO_BIN=$stress_bin VO_GC_STRESS=$every run_once "$base" "$work/stress"
        for s in out err code; do
            cmp -s "$pre.$s" "$work/stress.$s" || { ok=0; report="${report}stress run (every ${every}th allocation) differs in $s:
$(diff "$pre.$s" "$work/stress.$s" | head -20)
"; }
        done
    fi
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
    # folders with a `steps` file are step cases, run by tests/steps.py
    if [ -d "$p" ]; then find "$p" -type d -exec test -f '{}/steps' \; -prune -o \( -name '*.vo' -o -name '*.cmd' \) -print ; else echo "$p"; fi
done | sed -E 's/\.(vo|cmd)$//' | sort -u)

runnable=$(for base in $cases; do
    case $base in *.expected) continue ;; esac
    if [ -f "$base.vo" ]; then
        [ -f "$base.cmd" ] || [ -f "$base.out" ] || [ -f "$base.err" ] || [ -f "$base.code" ] || continue
    elif [ ! -f "$base.cmd" ]; then continue; fi
    echo "$base"
done)

# Worker k of $jobs: every jobs-th case from the k-th, in its own directories; prints its PASS/FAIL lines,
# then "passed P, failed F".
jobs=${VO_JOBS:-$(nproc 2>/dev/null || echo 1)}
worker() {
    tmp="$root/build/tmp.$1"; work="$root/build/run-work.$1"; mkdir -p "$work"
    echo "$runnable" | awk -v k="$1" -v j="$jobs" 'NF && NR % j == k % j' | {
        while read -r base; do check_case "$base"; done
        echo "passed $passed, failed $failed"
    }
}
k=1
while [ "$k" -le "$jobs" ]; do
    worker "$k" >"$work/worker.$k" 2>&1 &
    k=$((k + 1))
done
wait
k=1; passed=0; failed=0
while [ "$k" -le "$jobs" ]; do
    sed '$d' "$work/worker.$k"
    set -- $(tail -n 1 "$work/worker.$k" | tr -d ',')
    passed=$((passed + $2)); failed=$((failed + $4))
    k=$((k + 1))
done
echo "passed $passed, failed $failed"
[ "$failed" = 0 ]
