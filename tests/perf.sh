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
exit $fail
