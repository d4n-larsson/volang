#!/bin/sh
# SC-004 (v1) and SC-008 (v2): check + run of a 1,000-line program in under 1 s; large-input edge cases.
root=$(cd "$(dirname "$0")/.." && pwd)
vo="$root/bin/vo"
tmp="$root/build/tmp"
mkdir -p "$tmp"
python3 - "$tmp" <<'PY'
import sys, os
d = sys.argv[1]
lines = ["record Point {", "    x: int;", "    y: int;", "}", "",
         "variant Size {", "    small;", "    big(int);", "}", "",
         "fn classify(n: int) -> Size {", "    if n < 10 {", "        return Size.small;", "    }",
         "    return Size.big(n);", "}", ""]
for i in range(50):
    lines += [f"fn f{i}(n: int) -> int {{", "    var total = 0;", "    for k in 0..n {",
              "        if k % 2 == 0 {", "            set total = total + k;", "        } else {",
              "            set total = total - 1;", "        }", "    }",
              "    let p = new Point { x: total, y: n };", "    match classify(p.x) {",
              "        case small {", "            set total = total + 1;", "        }",
              "        case big(v) {", "            set total = total + v;", "        }", "    }",
              "    return total + p.y;", "}", ""]
lines += ["fn main() uses console {", "    var sum = 0;"]
lines += [f"    set sum = sum + f{i}({i});" for i in range(50)]
lines += ["    print(str(sum));", "}"]
open(os.path.join(d, "perf.vo"), "w").write("\n".join(lines) + "\n")
big = ["fn main() uses console {", "    var x = 0;"] + ["    set x = x + 1;"] * 10000 + ["    print(str(x));", "}"]
open(os.path.join(d, "big.vo"), "w").write("\n".join(big) + "\n")
open(os.path.join(d, "wide.vo"), "w").write('fn main() uses console {\n    let s = "' + "a" * 10000 + '";\n    print(str(len(s)));\n}\n')
v2 = ["record Box[T] {", "    value: T;", "}", "",
      "fn largest[T: ordered](xs: list[T], start: T) -> T {", "    var best = start;", "    for x in xs {",
      "        set best = max(best, x);", "    }", "    return best;", "}", "",
      "fn apply_all[E: effects](xs: list[int], f: fn(int) -> int uses E) -> list[int] uses E {",
      "    return map(xs, f);", "}", ""]
for i in range(40):
    v2 += [f"fn g{i}(n: int) -> int {{", f"    let base = n + {i};",
           "    let xs = apply_all([1, 2, 3, 4, 5, base], fn(x: int) -> int {", "        return x * 2 + base;", "    });",
           "    let evens = filter(xs, fn(x: int) -> bool {", "        return x % 2 == 0;", "    });",
           "    let total = fold(evens, 0, fn(acc: int, x: int) -> int {", "        return acc + x;", "    });",
           "    let b = new Box[int] { value: largest(xs, 0) };",
           "    sort_by(xs, fn(x: int) -> int {", "        return 0 - x;", "    });",
           "    let names = map(xs, fn(x: int) -> str {", "        return str(x);", "    });",
           "    return total + b.value + xs[0] + len(largest(names, \"\"));", "}", "",
           f"fn report{i}(n: int) uses console {{", f"    print(\"g{i} \" + str(g{i}(n)));", "}", ""]
v2 += ["fn main() uses console {"] + [f"    report{i}({i});" for i in range(40)] + ["}"]
open(os.path.join(d, "perf2.vo"), "w").write("\n".join(v2) + "\n")
PY
fail=0
lines=$(wc -l < "$tmp/perf.vo")
t0=$(date +%s%N)
"$vo" check "$tmp/perf.vo" >/dev/null || { echo "FAIL perf.vo does not check"; fail=1; }
t1=$(date +%s%N)
"$vo" run "$tmp/perf.vo" >/dev/null || { echo "FAIL perf.vo does not run"; fail=1; }
t2=$(date +%s%N)
check_ms=$(( (t1 - t0) / 1000000 )); run_ms=$(( (t2 - t1) / 1000000 )); total=$((check_ms + run_ms))
echo "perf: $lines lines, check ${check_ms} ms, run ${run_ms} ms, total ${total} ms (budget 1000 ms)"
[ "$total" -lt 1000 ] || { echo "FAIL SC-004: ${total} ms"; fail=1; }
out=$("$vo" run "$tmp/big.vo") && [ "$out" = 10000 ] && echo "big.vo (10,002 lines): ok" || { echo "FAIL big.vo: $out"; fail=1; }
out=$("$vo" run "$tmp/wide.vo") && [ "$out" = 10000 ] && echo "wide.vo (10,000-character literal): ok" || { echo "FAIL wide.vo: $out"; fail=1; }
lines2=$(wc -l < "$tmp/perf2.vo")
t0=$(date +%s%N)
"$vo" check "$tmp/perf2.vo" >/dev/null || { echo "FAIL perf2.vo does not check"; fail=1; }
t1=$(date +%s%N)
"$vo" run "$tmp/perf2.vo" >/dev/null || { echo "FAIL perf2.vo does not run"; fail=1; }
t2=$(date +%s%N)
check_ms=$(( (t1 - t0) / 1000000 )); run_ms=$(( (t2 - t1) / 1000000 )); total=$((check_ms + run_ms))
echo "perf (v2: effects, closures, generics): $lines2 lines, check ${check_ms} ms, run ${run_ms} ms, total ${total} ms (budget 1000 ms)"
[ "$total" -lt 1000 ] || { echo "FAIL SC-008: ${total} ms"; fail=1; }
# SC-008 (v3): a hot loop through a generic helper is at most 1.5 times slower than the
# type-specific version (median of three runs each).
python3 "$root/tests/perf/generic_pairs.py" "$tmp"
median_ms() {
    for i in 1 2 3; do
        t0=$(date +%s%N); "$vo" run --timeout 0 "$1" >/dev/null; t1=$(date +%s%N); echo $(( (t1 - t0) / 1000000 ))
    done | sort -n | sed -n 2p
}
for pair in max same stack; do
    g=$(median_ms "$tmp/${pair}_generic.vo"); s=$(median_ms "$tmp/${pair}_specific.vo")
    echo "generic $pair: ${g} ms vs specific ${s} ms (ratio $(awk -v g="$g" -v s="$s" 'BEGIN { printf "%.2f", g / s }'), limit 1.5)"
    [ $((g * 10)) -le $((s * 15)) ] || { echo "FAIL SC-008: generic $pair ${g} ms > 1.5 x ${s} ms"; fail=1; }
done

# Checking grows with the program, not with its square: four times the items (2,000 and 8,000
# records and functions) take at most 8 times as long (it was 34 times with names on one chain).
python3 - "$tmp" <<'PY'
import sys, os
for n in (2000, 8000):
    out = []
    for i in range(n):
        last = "x" if i % 200 == 0 else f"f{i - 1}(x) + 1"
        out += [f"record R{i} {{", "    a: int;", "    b: str;", "}", "",
                f"fn f{i}(x: int) -> int {{", f"    let r = new R{i} {{ a: x + {i}, b: \"s\" }};",
                "    if r.a > 10 {", "        return r.a * 2 + len(r.b);", "    }", f"    return {last};", "}", ""]
    out += ["fn main() uses console {", f"    print(str(f{n - 1}(1)));", "}"]
    open(os.path.join(sys.argv[1], f"items{n}.vo"), "w").write("\n".join(out) + "\n")
PY
check_ms() {
    for i in 1 2 3; do
        t0=$(date +%s%N); "$vo" check "$1" >/dev/null || echo "FAIL $1 does not check" >&2; t1=$(date +%s%N)
        echo $(( (t1 - t0) / 1000000 + 1 ))
    done | sort -n | sed -n 2p
}
small=$(check_ms "$tmp/items2000.vo"); large=$(check_ms "$tmp/items8000.vo")
echo "check scaling: 2,000 items ${small} ms, 8,000 items ${large} ms (limit 8 x)"
[ "$large" -le $((small * 8)) ] || { echo "FAIL check scaling: ${large} ms > 8 x ${small} ms"; fail=1; }

# SC-006 (v3): at most 20% slower than the baseline recorded before the collector
# (tests/gc/baseline/perf.txt), comparing the median of five check + run timings.
base="$root/tests/gc/baseline/perf.txt"
if [ -f "$base" ]; then
    for pair in "perf:perf.vo" "perf_(v2:perf2.vo"; do
        name=${pair%%:*}; prog=${pair#*:}
        want=$(awk -v n="$name" '$1 == n {print $2}' "$base")
        [ -n "$want" ] || continue
        got=$(for i in 1 2 3 4 5; do
            t0=$(date +%s%N); "$vo" check "$tmp/$prog" >/dev/null; "$vo" run "$tmp/$prog" >/dev/null; t1=$(date +%s%N)
            echo $(( (t1 - t0) / 1000000 ))
        done | sort -n | sed -n 3p)
        echo "$prog: median ${got} ms, baseline ${want} ms (limit +20%)"
        [ $((got * 100)) -le $((want * 120)) ] || { echo "FAIL SC-006: $prog ${got} ms > 1.2 x ${want} ms"; fail=1; }
    done
fi
exit $fail
