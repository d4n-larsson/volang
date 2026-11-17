#!/usr/bin/env python3
"""Spec 018 SC-001 and SC-004: listing and selecting tests at scale, and the cost of coverage.

SC-001: 200 tests and properties in 20 files are listed in under 1 s, and every listed identifier
selects exactly that test. SC-004: running the tests with --coverage takes at most 1.5 times as long
as without, on a compute-heavy suite (the median of 5 runs of each).
"""
import json
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))

FILE = """fn f{k}(n: int) -> int {{
    var t = 0;
    var i = 0;
    while i < n {{
        if i % 3 == 0 {{
            set t = t + i;
        }} else {{
            set t = t - 1;
        }}
        set i = i + 1;
    }}
    return t;
}}
"""

TEST = """
# Checks f{k} on {j}.
@spec("FR-{k:03d}/T{j}")
test "f{k} case {j}" {{
    expect(f{k}({j}) == f{k}({j}));
    expect_eq(f{k}({j}) + 0, f{k}({j}));
}}
"""

PROP = """
prop "f{k} is stable" (n: int) {{
    assume(n >= 0 and n < 50);
    expect_eq(f{k}(n), f{k}(n));
}}
"""

HEAVY = """fn work(n: int) -> int {
    var t = 0;
    var i = 0;
    while i < n {
        if i % 7 == 0 {
            set t = t + i % 13;
        } else {
            set t = t + 1;
        }
        set i = i + 1;
    }
    return t;
}

test "heavy" {
    expect(work(30000000) > 0);
}
"""


def run(args, cwd):
    start = time.monotonic()
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, text=True, timeout=600)
    return p, time.monotonic() - start


def main():
    bad = 0
    d = tempfile.mkdtemp()
    try:
        for k in range(20):
            body = FILE.format(k=k) + "".join(TEST.format(k=k, j=j) for j in range(9)) + PROP.format(k=k)
            open(os.path.join(d, f"t{k:02d}.vo"), "w").write(body)
        p, dt = run(["test", "--list", "--json", "."], d)
        entries = [json.loads(l) for l in p.stdout.splitlines() if '"test_entry"' in l]
        ok = p.returncode == 0 and len(entries) == 200 and dt < 1.0
        bad += not ok
        print(f"test_scale: listing {len(entries)} tests in 20 files: {dt:.2f} s (limit 1 s)"
              f"{'' if ok else ' FAILED: ' + p.stderr[-300:]}")
        wrong = 0
        for e in entries:
            p, _ = run(["test", "--json", "--test", e["id"], "."], d)
            ran = [json.loads(l)["id"] for l in p.stdout.splitlines() if '"kind":"test"' in l]
            wrong += ran != [e["id"]]
        bad += wrong != 0
        print(f"test_scale: every identifier selects exactly its test: {len(entries) - wrong} of {len(entries)}")
        open(os.path.join(d, "heavy.vo"), "w").write(HEAVY)
        plain = statistics.median(run(["test", "heavy.vo"], d)[1] for _ in range(5))
        cov = statistics.median(run(["test", "--coverage", "heavy.vo"], d)[1] for _ in range(5))
        ok = cov <= plain * 1.5
        bad += not ok
        print(f"test_scale: coverage overhead {cov / plain:.2f}x ({plain:.3f} s without, {cov:.3f} s with; "
              f"limit 1.5x){'' if ok else ' FAILED'}")
    finally:
        shutil.rmtree(d)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
