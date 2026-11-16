#!/usr/bin/env python3
"""Spec 014 SC-002/SC-003: the collections at 100,000 operations (timing-sensitive: not part of
`make test`; run alone).

- SC-002: 100,000 inserts then 100,000 lookups in the ordered map (random and ascending keys), the same
  in the ordered set, and 100,000 pushes then pops in the priority queue, each under 5 s; doubling the
  size at most about doubles the time (ratio at most 2.5, so no quadratic behavior).
- SC-003: the ordered map and set within 5x of the built-in `map` for the same inserts and lookups.

  structures_bench.py [VO]
"""
import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "vo"))
IMPORTS = ""  # the collections are built in
KEY = {"random": "(i * 7919) % N", "sorted": "i"}
PROGRAMS = {
    "omap": "let m = new ordered_map[int, int];\n"
            "for i in 0..N { set m[KEY] = i; }\nvar s = 0;\nfor i in 0..N { set s = s + get_or(m, KEY, 0); }",
    "oset": "let m = new ordered_set[int];\n"
            "for i in 0..N { push(m, KEY); }\nvar s = 0;\nfor i in 0..N { if has(m, KEY) { set s = s + 1; } }",
    "map": "let m = new map[int, int];\n"
           "for i in 0..N { set m[KEY] = i; }\nvar s = 0;\nfor i in 0..N { set s = s + get_or(m, KEY, 0); }",
    "pqueue": "let m = new priority_queue[int];\n"
              "for i in 0..N { push(m, KEY); }\nvar s = 0;\nwhile len(m) > 0 { match pop(m) { case ok(x) { set s = s + x; } case err(_) { } } }",
}


def seconds(kind, keys, n):
    body = PROGRAMS[kind].replace("KEY", KEY[keys]).replace("N", str(n))
    src = IMPORTS + "\nfn main() uses console {\n" + body + "\nprint(str(s));\n}\n"
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "b.vo"), "w").write(src)
        subprocess.run([VO, "check", "b.vo"], cwd=d, check=True, capture_output=True)
        best = None
        for _ in range(3):  # best of three
            t = time.time()
            r = subprocess.run([VO, "run", "--timeout", "60", "b.vo"], cwd=d, capture_output=True)
            took = time.time() - t
            if r.returncode:
                raise SystemExit(f"{kind} {keys} {n}: exit {r.returncode}\n{r.stderr.decode()}")
            best = took if best is None else min(best, took)
        return best


fails = []
times = {}
for kind in ("omap", "oset", "pqueue", "map"):
    for keys in ("random", "sorted"):
        a, b = seconds(kind, keys, 100000), seconds(kind, keys, 200000)
        times[kind, keys] = a
        print(f"{kind:7} {keys:6} 100,000: {a:6.3f} s   200,000: {b:6.3f} s   ratio {b / a:4.2f}")
        if kind != "map" and a >= 5:
            fails.append(f"SC-002: {kind} {keys} takes {a:.2f} s")
        if kind != "map" and b / a > 2.5:
            fails.append(f"SC-002: {kind} {keys} grows {b / a:.2f}x when doubled")
for kind in ("omap", "oset"):
    for keys in ("random", "sorted"):
        r = times[kind, keys] / times["map", keys]
        print(f"SC-003: {kind} {keys} is {r:.2f}x the built-in map")
        if r > 5:
            fails.append(f"SC-003: {kind} {keys} is {r:.2f}x the built-in map")
for f in fails:
    print("FAIL", f)
print(f"structures_bench: {len(fails)} failures")
sys.exit(1 if fails else 0)
