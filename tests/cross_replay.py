#!/usr/bin/env python3
"""SC-007 (spec 008 T058): a recording made by one build replays byte-identically on another.

  cross_replay.py [--record BUILD_A --replay BUILD_B]

Runs every replay step case (tests/cases/replay, and spec 016's tests/cases/tasks_steps/record_replay, spec 017's tests/cases/fs_steps/record_replay, spec 019's tests/cases/https_steps/record_replay) with its `vo run --record` steps on BUILD_A and its
`vo replay` steps on BUILD_B, against the case's expected output. The default runs every ordered pair
of build/vo, build-aarch64/vo and build-riscv64/vo that exist (bin/vo runs foreign ones under qemu).
"""
import itertools
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = [os.path.join(ROOT, "tests", "cases", "replay"), os.path.join(ROOT, "tests", "cases", "tasks_steps", "record_replay"),
         os.path.join(ROOT, "tests", "cases", "fs_steps", "record_replay"),  # spec 017: files and a run
         os.path.join(ROOT, "tests", "cases", "https_steps", "record_replay")]  # spec 019: HTTPS


def arg(name):
    return os.path.abspath(sys.argv[sys.argv.index(name) + 1]) if name in sys.argv else None


if arg("--record"):
    pairs = [(arg("--record"), arg("--replay"))]
else:
    builds = [os.path.join(ROOT, b, "vo") for b in ("build", "build-aarch64", "build-riscv64")]
    pairs = list(itertools.permutations([b for b in builds if os.path.exists(b)], 2))
bad = 0
for a, b in pairs:
    name = f"{os.path.relpath(a, ROOT)} -> {os.path.relpath(b, ROOT)}"
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tests", "steps.py")] + CASES,
                       env=dict(os.environ, VO_BIN=a, VO_REPLAY_BIN=b), capture_output=True, text=True)
    last = (r.stdout.strip().splitlines() or ["(no output)"])[-1]
    print(f"{'ok  ' if r.returncode == 0 else 'FAIL'} {name}: {last}")
    if r.returncode:
        print(r.stdout[-3000:] + r.stderr[-2000:])
        bad += 1
print(f"cross_replay: {len(pairs)} pairs, " + ("all identical" if not bad else f"{bad} failed"))
sys.exit(1 if bad else 0)
