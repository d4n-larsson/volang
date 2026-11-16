#!/usr/bin/env python3
"""Spec 016 SC-001 and SC-003: concurrent runs are byte-identical whatever the response timing.

  tasks_determinism.py [--runs N] [--jitter MS] [--timing]

Runs the golden cases of tests/cases/tasks/ and the step cases of tests/cases/tasks_steps/ N times
(default 100; each run must match the expectation and the first run), with the test server adding a
random 0..MS ms (default 300) to every /delay response, so tasks finish in a different order each
time. --timing also measures quickstart.md scenario 1: ten /delay/1000 requests with
`concurrent_map` (limit 10) and with `map` (SC-001: under 3 s, at least 10 s).
"""
import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))

FETCH = """import std.http.get;

fn fetch(i: int) -> str uses net("127.0.0.1") {
    match get("http://127.0.0.1:PORT/delay/1000") {
        case ok(r) {
            return str(i) + " " + str(r.status);
        }
        case err(e) {
            return str(i) + " " + e;
        }
    }
}

fn main() uses console, net("127.0.0.1") {
    for line in CALL {
        print(line);
    }
}
"""


def timing():
    server = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "http_server.py"), "0"],
                              stdout=subprocess.PIPE, text=True)
    port = server.stdout.readline().split()[1]
    try:
        times = {}
        with tempfile.TemporaryDirectory() as d:
            for name, call in (("concurrent", "concurrent_map([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 10, fetch)"),
                               ("sequential", "map([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], fetch)")):
                path = os.path.join(d, name + ".vo")
                with open(path, "w") as f:
                    f.write(FETCH.replace("PORT", port).replace("CALL", call))
                start = time.monotonic()
                r = subprocess.run([VO, "run", "--timeout", "60", path], capture_output=True, text=True)
                times[name] = time.monotonic() - start
                lines = r.stdout.split("\n")[:-1]
                if r.returncode or lines != [f"{i} 200" for i in range(10)]:
                    print(f"timing: {name} run failed: {r.returncode} {r.stdout!r} {r.stderr!r}")
                    return 1
        ok = times["concurrent"] < 3 and times["sequential"] >= 10
        print(f"timing: concurrent {times['concurrent']:.2f} s, sequential {times['sequential']:.2f} s "
              f"({'met' if ok else 'NOT met'}: under 3 s and at least 10 s)")
        return 0 if ok else 1
    finally:
        server.kill()


def main():
    args = sys.argv[1:]
    runs, jitter = 100, 300
    if "--runs" in args:
        runs = int(args[args.index("--runs") + 1])
    if "--jitter" in args:
        jitter = int(args[args.index("--jitter") + 1])
    env = dict(os.environ, VO_TEST_JITTER_MS=str(jitter))
    fail = 0
    for cmd in ([os.path.join(ROOT, "tests", "run.sh"), "--repeat", str(runs), os.path.join(ROOT, "tests", "cases", "tasks")],
                [sys.executable, os.path.join(ROOT, "tests", "steps.py"), "--repeat", str(runs),
                 os.path.join(ROOT, "tests", "cases", "tasks_steps")]):
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        last = (r.stdout.strip().split("\n") or [""])[-1]
        print(f"{os.path.basename(cmd[0] if cmd[0] != sys.executable else cmd[1])}: {runs} runs each, jitter {jitter} ms: {last}")
        if r.returncode:
            print("\n".join(l for l in r.stdout.split("\n") if not l.startswith("PASS")))
            fail = 1
    if "--timing" in args:
        fail |= timing()
    return fail


if __name__ == "__main__":
    sys.exit(main())
