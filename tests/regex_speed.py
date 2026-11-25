#!/usr/bin/env python3
"""Spec 025 SC-002 and SC-003: std.regex runs in linear time.

SC-002: patterns that make backtracking engines take exponential time, on 100,000 and 200,000
characters: each under 1 s, and doubling the text at most 2.5 times the time.
SC-003: all matches of a date-and-level pattern in 10 MB of generated log lines: under 2 s.
Figures are the median of 3 runs of `vo run`, process start included. Needs an idle machine: not in
`make test`, in `make gate`.
"""
import os
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

# (pattern as the engine sees it, the unit the text repeats, the text's last character)
PATHOLOGICAL = [
    (r"(a+)+$", "a", "b"),
    (r"(a|a)*b", "a", "c"),
    (r"(.*)*x", "a", "y"),
    (r"(a*)*b", "a", "c"),
    (r"(\w+\s?)*$", "ab ", "!"),
]


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def program(pattern, unit, last, n):
    return f"""import std.regex;
import std.text;

fn main() uses console {{
    let subject = text.repeat({vo_str(unit)}, {n // len(unit)}) + {vo_str(last)};
    print(str(regex.is_match(subject, regex.regex({vo_str(pattern)}))));
}}
"""


LOG = """import std.regex;
import std.fs;
import std.bytes;

fn main() -> result[void] uses console, file.read("logs/") {{
    let text = try bytes.to_text(try fs.read_bytes("logs/app.log"));
    let re = regex.regex("(\\\\d{{4}})-(\\\\d{{2}})-(\\\\d{{2}}) \\\\S+ (ERROR|WARN)");
    print(str(len(regex.all_matches(text, re))));
    return ok();
}}
"""


def timed(path, cwd=None):
    ts = []
    for _ in range(3):
        t0 = time.perf_counter()
        p = subprocess.run([VO, "run", path], capture_output=True, cwd=cwd)
        ts.append(time.perf_counter() - t0)
        if p.returncode != 0:
            sys.exit(f"regex_speed: {path} exited {p.returncode}: {p.stderr.decode(errors='replace')[:2000]}")
    return statistics.median(ts), p.stdout.decode().strip()


def main():
    tmp = tempfile.mkdtemp(prefix="voregexspeed-")
    fails = 0
    try:
        for pattern, unit, last in PATHOLOGICAL:
            times = []
            for n in (100000, 200000):
                path = os.path.join(tmp, f"p{n}.vo")
                open(path, "w").write(program(pattern, unit, last, n))
                t, _ = timed(path)
                times.append(t)
            ok = times[0] < 1.0 and times[1] < 1.0 and times[1] <= 2.5 * times[0]
            fails += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {pattern}: {times[0]:.3f} s for 100,000, {times[1]:.3f} s for 200,000"
                  f" ({times[1] / times[0]:.2f}x)")
        lines = []
        size = 0
        i = 0
        levels = ["INFO", "DEBUG", "ERROR", "WARN", "INFO", "INFO"]
        while size < 10_000_000:
            line = f"2026-11-{1 + i % 28:02d} 12:{i % 60:02d}:{(i * 7) % 60:02d} {levels[i % 6]} request {i} took {i % 997} ms\n"
            lines.append(line)
            size += len(line)
            i += 1
        os.makedirs(os.path.join(tmp, "logs"))
        log = os.path.join(tmp, "logs", "app.log")
        open(log, "w").write("".join(lines))
        path = os.path.join(tmp, "log.vo")
        open(path, "w").write(LOG.format())
        t, out = timed(path, cwd=tmp)
        want = sum(1 for k in range(i) if levels[k % 6] in ("ERROR", "WARN"))
        ok = t < 2.0 and out == str(want)
        fails += not ok
        print(f"{'ok  ' if ok else 'FAIL'} 10 MB of logs: {t:.3f} s, {out} matches (expected {want})")
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"regex_speed: {fails} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
