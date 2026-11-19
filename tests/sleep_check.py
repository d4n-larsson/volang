#!/usr/bin/env python3
"""Spec 022 SC-001 to SC-005 (and FR-007): the timed promises of std.time.sleep. Needs an idle machine,
so it runs in `make gate`, not `make test`. Each figure is the median of 5 whole `vo run` processes.

- SC-001: sleep(200) adds 200-300 ms of real time and under 10 ms of CPU time;
- SC-002: a test suite that sleeps an hour of fixed-clock time finishes in under 1 s;
- SC-003: replaying a recorded run that slept 2 s takes under 500 ms;
- SC-004: sleep(60000) under --timeout 1 stops with R0009 (exit 124) within 1.5 s;
- SC-005: ten concurrent tasks sleeping 300 ms each finish in under 1.5 s;
- FR-007: SIGTERM ends a sleeping program within 0.5 s.
"""
import os, resource, signal, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))
SLEEPY = """import std.time.sleep;

fn main() uses console, clock {{
    sleep({ms});
    print("done");
}}
"""


def timed(args, cwd, runs=5):
    """(median elapsed s, median child CPU s, last exit code)"""
    walls, cpus, code = [], [], 0
    for _ in range(runs):
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        t = time.monotonic()
        p = subprocess.run([VO] + args, cwd=cwd, capture_output=True)
        walls.append(time.monotonic() - t)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        cpus.append(after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime)
        code = p.returncode
    return statistics.median(walls), statistics.median(cpus), code


def write(d, name, text):
    open(os.path.join(d, name), "w").write(text)
    return name


def main():
    fails = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text)
        if not ok:
            fails.append(text)

    with tempfile.TemporaryDirectory() as d:
        base = timed(["run", write(d, "s0.vo", SLEEPY.format(ms=0))], d)
        slept = timed(["run", write(d, "s200.vo", SLEEPY.format(ms=200))], d)
        extra, cpu = slept[0] - base[0], slept[1] - base[1]
        check(0.2 <= extra <= 0.3 and cpu < 0.01, f"SC-001: sleep(200) adds {extra * 1000:.0f} ms, {cpu * 1000:.1f} ms CPU")
        hour = timed(["test", os.path.join(ROOT, "tests", "cases", "time", "sleep_test_hour.vo")], d)
        check(hour[0] < 1.0 and hour[2] == 0, f"SC-002: an hour of fixed-clock sleeps in {hour[0] * 1000:.0f} ms")
        main_vo = write(d, "r.vo", SLEEPY.format(ms=2000))
        subprocess.run([VO, "run", "--record", "r.jsonl", main_vo], cwd=d, capture_output=True)
        rep = timed(["replay", "r.jsonl"], d)
        check(rep[0] < 0.5 and rep[2] == 0, f"SC-003: replay of a 2 s sleep in {rep[0] * 1000:.0f} ms")
        to = timed(["run", "--timeout", "1", write(d, "t.vo", SLEEPY.format(ms=60000))], d, runs=3)
        check(to[0] < 1.5 and to[2] == 124, f"SC-004: R0009 after {to[0]:.2f} s (exit {to[2]})")
        tasks = timed(["run", os.path.join(ROOT, "tests", "cases", "sleep_steps", "tasks", "main.vo")], d)
        check(tasks[0] < 1.5 and tasks[2] == 0, f"SC-005: ten 300 ms sleeps in tasks took {tasks[0]:.2f} s")
        p = subprocess.Popen([VO, "run", "t.vo"], cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.3)
        t = time.monotonic()
        p.send_signal(signal.SIGTERM)
        p.wait(timeout=5)
        took = time.monotonic() - t
        check(took < 0.5, f"FR-007: SIGTERM ended a sleeping program in {took * 1000:.0f} ms (status {p.returncode})")
    print(f"sleep_check: {len(fails)} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
