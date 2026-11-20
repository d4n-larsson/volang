#!/usr/bin/env python3
"""Spec 034 SC-001–SC-003: every example as one executable.

- SC-001: each examples/**/main.vo is built with `vo build`; every recording under its recordings/
  is replayed with `vo replay` (in the example's directory) and with the executable's
  `--vo-replay` (from an empty directory): stdout, stderr and exit status must be equal.
- SC-002: the expenses example, copied, is run by its executable with `--vo-record`; the executable
  then replays that recording from an empty directory to the same output.
- SC-003: a second build of each example is byte-identical.

  build_examples.py            the checks (part of `make test`)
  build_examples.py --time     SC-004: start time of an executable against `vo run` (timing-sensitive)
"""
import glob
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN (default build/vo) natively or under qemu


def vo(args, cwd, **kw):
    return subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=120, **kw)


def app(path, args, cwd, **kw):
    """Runs a built executable the way bin/vo runs the toolchain (so cross builds run under qemu)."""
    return subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=120,
                          env=dict(os.environ, VO_BIN=path), **kw)


def build(example_dir, out):
    r = vo(["build", "main.vo", "-o", out], example_dir)
    if r.returncode:
        raise SystemExit(f"vo build fails in {example_dir}:\n{r.stdout.decode()}{r.stderr.decode()}")


def same(a, b):
    return (a.stdout, a.stderr, a.returncode) == (b.stdout, b.stderr, b.returncode)


def timing(work):
    """SC-004: replaying the same recording, `vo replay` (which checks, then runs, as `vo run`) against
    the executable; paired, the order alternating, median of the per-round ratios."""
    d = os.path.join(ROOT, "examples", "notes-api")
    rec = os.path.join(d, "recordings", "demo.jsonl")
    exe = os.path.join(work, "timed")
    build(d, exe)
    empty = os.path.join(work, "empty")

    def once(f):
        t = time.perf_counter()
        f()
        return time.perf_counter() - t

    runs = {"vo": lambda: vo(["replay", rec], d), "app": lambda: app(exe, ["--vo-replay", rec], empty)}
    times = {"vo": [], "app": []}
    for r in range(15):
        for k in (("vo", "app") if r % 2 == 0 else ("app", "vo")):
            times[k].append(once(runs[k]))
    ratio = statistics.median(a / v for v, a in zip(times["vo"], times["app"]))
    print(f"SC-004 notes-api replay: vo {statistics.median(times['vo']) * 1000:.1f} ms  "
          f"executable {statistics.median(times['app']) * 1000:.1f} ms  paired ratio {ratio:.3f}")
    return ratio <= 1.05


def main():
    fails = []
    work = tempfile.mkdtemp(prefix="vo-build-")
    try:
        empty = os.path.join(work, "empty")
        os.mkdir(empty)
        if "--time" in sys.argv:
            sys.exit(0 if timing(work) else 1)
        mains = sorted(glob.glob(os.path.join(ROOT, "examples", "*", "main.vo")) +
                       glob.glob(os.path.join(ROOT, "examples", "*", "*", "main.vo")))
        replays = 0
        for i, m in enumerate(mains):
            d = os.path.dirname(m)
            name = os.path.relpath(d, os.path.join(ROOT, "examples"))
            exe, again = os.path.join(work, f"app{i}"), os.path.join(work, f"again{i}")
            build(d, exe)
            build(d, again)
            if open(exe, "rb").read() != open(again, "rb").read():
                fails.append(f"SC-003: two builds of {name} differ")
            for rec in sorted(glob.glob(os.path.join(d, "recordings", "*.jsonl"))):
                replays += 1
                a = vo(["replay", os.path.relpath(rec, d)], d)
                b = app(exe, ["--vo-replay", rec], empty)
                if not same(a, b):
                    fails.append(f"SC-001: {name} {os.path.basename(rec)}: the executable replays differently\n"
                                 f"  vo replay: exit {a.returncode} {a.stdout[-200:]!r} {a.stderr[-200:]!r}\n"
                                 f"  executable: exit {b.returncode} {b.stdout[-200:]!r} {b.stderr[-200:]!r}")
        # SC-002: record with the executable, replay with it elsewhere
        copy = os.path.join(work, "expenses")
        shutil.copytree(os.path.join(ROOT, "examples", "expenses"), copy)
        exe = os.path.join(work, "expenses-app")
        build(copy, exe)
        rec = os.path.join(work, "run.jsonl")
        ran = app(exe, ["--vo-record", rec], copy)
        direct = vo(["run", "main.vo"], copy)
        if not same(ran, direct):
            fails.append("SC-001: expenses: the executable's run differs from `vo run`")
        back = app(exe, ["--vo-replay", rec], empty)
        if (back.stdout, back.returncode) != (ran.stdout, ran.returncode) or back.stderr:
            fails.append(f"SC-002: expenses: the executable replays its own recording differently: "
                         f"exit {back.returncode} {back.stderr[-300:]!r}")
        for f in fails:
            print("FAIL", f)
        print(f"build_examples: {len(mains)} examples built twice, {replays} recordings replayed, "
              f"1 run recorded and replayed, {len(fails)} failures")
        sys.exit(1 if fails else 0)
    finally:
        shutil.rmtree(work, ignore_errors=True)


main()
