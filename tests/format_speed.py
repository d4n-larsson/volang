#!/usr/bin/env python3
"""Spec 024 SC-004: `format` is no slower than the `+`/`str` code it replaces.

Each compared program does 5,000,000 iterations (fewer let process start-up decide). The two programs
of a pair are run in turns, 9 rounds, the order alternating, and the figure is the median of the
per-round ratios: a machine whose speed drifts during the check (a shared CI runner) then slows both
programs of a round alike, where timing one program five times and then the other does not. A `format`
program fails when that ratio is over 1.05. `{:.2f}` has no
equivalent before spec 024 and is timed for the record. Needs an idle machine: not in `make test`,
in `make gate`.
"""
import os
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
N = 5000000
N_RECORD = 1000000


def prog(imports, expr, n=N):
    return (f"{imports}fn main() uses console {{\n    var total = 0;\n    for i in 0..{n} {{\n"
            f"        set total = total + len({expr});\n    }}\n    print(str(total));\n}}\n")


PAIRS = [
    ("two values", prog("", 'format("{} {}", i, i + 1)'), prog("", 'str(i) + " " + str(i + 1)')),
    ("a column", prog("", 'format("{:>10}", i)'),
     prog("import std.text.align_right;\n\n", "align_right(str(i), 10)")),
]
RECORD = [("two decimals", prog("", 'format("{:.2f}", float(i) / 7.0)', N_RECORD))]


def once(path):
    t0 = time.perf_counter()
    p = subprocess.run([VO, "run", path], capture_output=True)
    t = time.perf_counter() - t0
    if p.returncode != 0:
        sys.exit(f"format_speed: {path} exited {p.returncode}: {p.stderr.decode(errors='replace')}")
    return t, p.stdout


def timed(path):
    runs = [once(path) for _ in range(5)]
    return statistics.median(t for t, _ in runs), runs[-1][1]


def paired(a, b, rounds=9):
    """Median time of each, their outputs, and the median of the per-round ratios a/b."""
    ta, tb, out = [], [], {}
    for r in range(rounds):
        for path, times in ((a, ta), (b, tb)) if r % 2 == 0 else ((b, tb), (a, ta)):
            t, out[path] = once(path)
            times.append(t)
    return statistics.median(ta), statistics.median(tb), out[a], out[b], statistics.median(x / y for x, y in zip(ta, tb))


def main():
    tmp = tempfile.mkdtemp(prefix="vofmtspeed-")
    fails = 0
    try:
        def write(name, src):
            path = os.path.join(tmp, name + ".vo")
            open(path, "w").write(src)
            return path
        for i, (name, fmt, old) in enumerate(PAIRS):
            tf, to, of, oo, ratio = paired(write(f"f{i}", fmt), write(f"o{i}", old))
            ok = ratio <= 1.05 and of == oo
            fails += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {name}: format {tf:.3f} s, + and str {to:.3f} s (paired {ratio:.2f}x)"
                  + ("" if of == oo else ", outputs differ"))
        for i, (name, src) in enumerate(RECORD):
            t, _ = timed(write(f"r{i}", src))
            print(f"     {name}: format {t:.3f} s for {N_RECORD:,} values")
    finally:
        for n in os.listdir(tmp):
            os.remove(os.path.join(tmp, n))
        os.rmdir(tmp)
    print(f"format_speed: {fails} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
