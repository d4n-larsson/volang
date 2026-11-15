#!/usr/bin/env python3
"""Spec 004 speed benchmark (SC-001, SC-002, SC-006).

  speed.py           time tests/speed/*.vo on build-oracle/vo (the tree walker), bin/vo and python3
  speed.py --bench2  CPU time of every bench2 reference run, oracle vs new (SC-006)
  speed.py --baseline PATH  each tests/speed program on PATH vs build/vo, alternated runs (spec 005 SC-006)
  --new PATH         time another build as "new" (default build/vo)

Exit status 1 when a target is missed: geometric-mean speedup < 4.0, any program < 2.0,
int_loop > 1350 ms; or (--bench2) total CPU > +10%, or one run slower by more than max(10%, 5 ms)
(median of 5 alternated runs, then of 15 for a run still over the limit); or (--baseline) any
program more than 5% slower than the baseline binary, by the same median rule; with --max-geomean R
(spec 008 SC-004), only the geometric mean of the per-program ratios must stay at or below R.
"""
import importlib.util
import math
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEED = os.path.join(ROOT, "tests", "speed")
ORACLE = os.path.join(ROOT, "build-oracle", "vo")
NEW = os.path.join(ROOT, "build", "vo")
VO = os.path.join(ROOT, "bin", "vo")
RUNS = 5


def vo_env(binary):
    return dict(os.environ, VO_BIN=binary)


def timed(cmd, env=None, cwd=None, stdin=None):
    """(wall ms, cpu ms of the child, stdout bytes, exit status)."""
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, env=env, cwd=cwd, stdin=subprocess.PIPE if stdin is not None else None,
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    out = b""
    if stdin is not None:
        p.stdin.write(stdin)
        p.stdin.close()
    out = p.stdout.read()
    _, status, ru = os.wait4(p.pid, 0)
    p.returncode = os.waitstatus_to_exitcode(status)
    wall = (time.perf_counter() - t0) * 1000
    return wall, (ru.ru_utime + ru.ru_stime) * 1000, out, p.returncode


def speed():
    names = sorted(f[:-3] for f in os.listdir(SPEED) if f.endswith(".vo"))
    ok = True
    ratios = []
    print(f"{'program':20} {'oracle ms':>10} {'new ms':>8} {'speedup':>8} {'python ms':>10} {'vo/py':>6}")
    for n in names:
        want = open(os.path.join(SPEED, n + ".out"), "rb").read()
        res = {}
        for label, cmd, env in (("oracle", [VO, "run", n + ".vo"], vo_env(ORACLE)),
                                ("new", [VO, "run", n + ".vo"], vo_env(NEW)),
                                ("python", ["python3", n + ".py"], None)):
            times = []
            for _ in range(RUNS):
                wall, _, out, code = timed(cmd, env=env, cwd=SPEED)
                if out != want or code != 0:
                    print(f"{n}: {label} printed {out[:80]!r} (exit {code}), want {want[:80]!r}")
                    ok = False
                times.append(wall)
            res[label] = statistics.median(times)
        sp = res["oracle"] / res["new"]
        ratios.append(sp)
        print(f"{n:20} {res['oracle']:10.0f} {res['new']:8.0f} {sp:7.2f}x {res['python']:10.0f} {res['new'] / res['python']:6.2f}")
        if sp < 2.0:
            print(f"  FAIL {n}: speedup {sp:.2f} < 2.0")
            ok = False
        if n == "int_loop" and res["new"] > 1350:
            print(f"  FAIL int_loop: {res['new']:.0f} ms > 1350 ms")
            ok = False
    geo = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
    print(f"geometric-mean speedup {geo:.2f}x")
    if geo < 4.0:
        print("  FAIL geometric mean < 4.0")
        ok = False
    return ok


def bench2():
    spec = importlib.util.spec_from_file_location("baseline", os.path.join(ROOT, "tests", "gc", "baseline.py"))
    baseline = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(baseline)
    jobs = list(baseline.jobs())

    def run(job, binary):
        task, tdir, case, inp = job
        data = open(inp, "rb").read()
        with tempfile.TemporaryDirectory() as tmp:
            fx = os.path.join(tdir, "fixtures")
            if os.path.isdir(fx):
                shutil.copytree(fx, tmp, dirs_exist_ok=True)
            shutil.copytree(os.path.join(tdir, "ref", "vo"), tmp, dirs_exist_ok=True)
            return timed([VO, "run", "--timeout", "120", "main.vo"], env=vo_env(binary), cwd=tmp, stdin=data)[1]

    tot = {"oracle": 0.0, "new": 0.0}
    slow = []
    for j in jobs:
        a, b = run(j, ORACLE), run(j, NEW)
        tot["oracle"] += a
        tot["new"] += b
        if b > max(a * 1.10, a + 5):
            slow.append(j)
    ok = True
    for j in slow:
        # Alternated runs, so load drift cancels. A median of 5 still moves by about 5 ms on a
        # loaded machine, so a run over the limit gets a second look with 15 pairs.
        for n in (RUNS, 15):
            pairs = [(run(j, ORACLE), run(j, NEW)) for _ in range(n)]
            a = statistics.median(x for x, _ in pairs)
            b = statistics.median(y for _, y in pairs)
            if b <= max(a * 1.10, a + 5):
                break
        else:
            print(f"  FAIL {j[0]} {j[2]}: {b:.0f} ms CPU vs {a:.0f} ms")
            ok = False
    print(f"bench2: {len(jobs)} runs, CPU total oracle {tot['oracle']:.0f} ms, new {tot['new']:.0f} ms "
          f"({tot['new'] / tot['oracle']:.2f}x), {len(slow)} re-timed")
    if tot["new"] > tot["oracle"] * 1.10:
        print("  FAIL total CPU more than 10% higher")
        ok = False
    return ok


def baseline(base, max_geo=None):
    ok = True
    ratios = []
    print(f"{'program':20} {'base ms':>8} {'new ms':>8} {'ratio':>6}")
    for n in sorted(f[:-3] for f in os.listdir(SPEED) if f.endswith(".vo")):
        def run(binary):
            return timed([VO, "run", n + ".vo"], env=vo_env(binary), cwd=SPEED)[0]
        for k in (RUNS, 15):
            pairs = [(run(base), run(NEW)) for _ in range(k)]
            a = statistics.median(x for x, _ in pairs)
            b = statistics.median(y for _, y in pairs)
            if b <= a * 1.05:
                break
        print(f"{n:20} {a:8.0f} {b:8.0f} {b / a:6.3f}")
        ratios.append(b / a)
        if max_geo is None and b > a * 1.05:
            print(f"  FAIL {n}: {b / a:.3f}x the baseline")
            ok = False
    if max_geo is not None:
        geo = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
        print(f"geometric-mean ratio {geo:.3f} (limit {max_geo})")
        if geo > max_geo:
            print("  FAIL geometric-mean ratio over the limit")
            ok = False
    return ok


def main():
    global NEW
    if "--new" in sys.argv:
        NEW = os.path.abspath(sys.argv[sys.argv.index("--new") + 1])
    if "--baseline" in sys.argv:
        base = os.path.abspath(sys.argv[sys.argv.index("--baseline") + 1])
        if not os.path.exists(base):
            print(f"speed: {base} is missing; run `make baseline-004`")
            return 1
        geo = float(sys.argv[sys.argv.index("--max-geomean") + 1]) if "--max-geomean" in sys.argv else None
        return 0 if baseline(base, geo) else 1
    if not os.path.exists(ORACLE):
        print("speed: build-oracle/vo is missing; run `make oracle`")
        return 1
    ok = bench2() if "--bench2" in sys.argv[1:] else speed()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
