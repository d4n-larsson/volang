#!/usr/bin/env python3
"""Spec 020 SC-006: one million std.math calls (pow, sin and log, a third each, the hardest on x86-64,
whose fma is software) finish in under 1,000 ms: the median of 5 whole `vo run` processes, native
build. The time without the math calls (the same loop) is printed for comparison."""
import os, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))
PROGRAM = """import std.math;

fn main() uses console {{
    var s = 0.0;
    var i = 0;
    while i < 333334 {{
        let x = float(i) * 0.001 + 0.5;
        s = s + {body};
        set i = i + 1;
    }}
    print(str(s));
}}
"""


def median_ms(path):
    times = []
    for _ in range(5):
        t = time.monotonic()
        p = subprocess.run([VO, "run", path], capture_output=True, text=True)
        times.append((time.monotonic() - t) * 1000)
        if p.returncode != 0:
            print(p.stderr)
            sys.exit(1)
    return statistics.median(times), p.stdout.strip()


def main():
    with tempfile.TemporaryDirectory() as d:
        math_p, loop_p = os.path.join(d, "math.vo"), os.path.join(d, "loop.vo")
        with open(math_p, "w") as f:
            f.write(PROGRAM.format(body="math.pow(x, 1.5) + math.sin(x) + math.log(x)").replace("s = s +", "set s = s +"))
        with open(loop_p, "w") as f:
            f.write(PROGRAM.format(body="x + x + x").replace("s = s +", "set s = s +"))
        m, out = median_ms(math_p)
        l, _ = median_ms(loop_p)
    print(f"math_speed: 1,000,002 calls in {m:.0f} ms (the loop alone {l:.0f} ms), checksum {out}; limit 1000 ms")
    return 0 if m < 1000 else 1


if __name__ == "__main__":
    sys.exit(main())
