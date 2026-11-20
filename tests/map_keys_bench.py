#!/usr/bin/env python3
"""Spec 033 SC-002/SC-003: map speed with the new keys (timing-sensitive: not part of `make test`;
run alone).

- SC-002: 1,000,000 inserts, lookups and removes with `int` keys and with `str` keys (the keys built
  before timing starts), OLD against NEW in pairs (the order alternating each round); the median of the
  per-pair NEW/OLD ratios at most 1.05. Pairing cancels the machine's drift, which a ratio of two
  separate medians does not.
- SC-003 (NEW only): the same map keyed by a two-field record against a `str` key built from the two
  fields, each key made at each access as programs do; the record no slower than the joined `str`.

  map_keys_bench.py OLD NEW      (OLD: build/vo-before, saved before the change)
"""
import os
import statistics
import subprocess
import sys
import tempfile
import time

OLD, NEW = (os.path.abspath(p) for p in sys.argv[1:3])
N = 1000000
RUNS = 9

LOOP = """
    let m = new map[K, int];
    for i in 0..N { set m[KEY] = i; }
    var s = 0;
    for i in 0..N { set s = s + get_or(m, KEY, 0); }
    for i in 0..N { remove(m, KEY); }
    print(str(s) + " " + str(len(m)));
"""
PROGRAMS = {
    "int": ("", "let ks = new list[int];\nfor i in 0..N { push(ks, i * 7919); }", "int", "ks[i]"),
    "str": ("", "let ks = new list[str];\nfor i in 0..N { push(ks, \"k\" + str(i)); }", "str", "ks[i]"),
    "record": ("record P {\n    x: int;\n    y: int;\n}\n", "", "P", "new P { x: i % 1000, y: i / 1000 }"),
    "joined": ("", "", "str", "str(i % 1000) + \",\" + str(i / 1000)"),
}


def source(kind):
    decls, setup, k, key = PROGRAMS[kind]
    body = LOOP.replace("KEY", key).replace("K,", k + ",").replace("N", str(N))
    return decls + "fn main() uses console {\n" + setup.replace("N", str(N)) + body + "}\n"


def once(vo, d):
    t = time.perf_counter()
    r = subprocess.run([vo, "run", "--timeout", "120", "b.vo"], cwd=d, capture_output=True)
    took = time.perf_counter() - t
    if r.returncode:
        raise SystemExit(f"{vo}: exit {r.returncode}\n{r.stderr.decode()}")
    return took, r.stdout


def runs(kind, binaries):
    """Seconds per binary for each round, the order alternating each round so drift hits each alike."""
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "b.vo"), "w").write(source(kind))
        times = {vo: [] for vo in binaries}
        outs = set()
        for r in range(RUNS):
            for vo in (binaries if r % 2 == 0 else binaries[::-1]):
                t, out = once(vo, d)
                times[vo].append(t)
                outs.add(out)
        if len(outs) != 1:
            raise SystemExit(f"{kind}: the binaries print different output")
        return times


def accepts(vo, kind):
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "b.vo"), "w").write(source(kind))
        return subprocess.run([vo, "check", "b.vo"], cwd=d, capture_output=True).returncode == 0


fails = []
for kind in ("int", "str"):
    t = runs(kind, [OLD, NEW])
    r = statistics.median(n / o for o, n in zip(t[OLD], t[NEW]))
    print(f"SC-002 {kind:4} keys: old {statistics.median(t[OLD]):.3f} s  new {statistics.median(t[NEW]):.3f} s  "
          f"paired ratio {r:.3f}")
    if r > 1.05:
        fails.append(f"SC-002: {kind} keys {r:.3f}x")
if accepts(NEW, "record"):
    rec = statistics.median(runs("record", [NEW])[NEW])
    joined = statistics.median(runs("joined", [NEW])[NEW])
    print(f"SC-003 record key {rec:.3f} s  joined str key {joined:.3f} s  ratio {rec / joined:.3f}")
    if rec > joined:
        fails.append(f"SC-003: record key {rec / joined:.3f}x the joined str key")
else:
    print("SC-003 skipped: NEW does not accept record keys")
for f in fails:
    print("FAIL", f)
print(f"map_keys_bench: {len(fails)} failures")
sys.exit(1 if fails else 0)
