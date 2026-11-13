#!/usr/bin/env python3
"""The "before the collector" baseline (spec 003 T001, SC-002, SC-003, SC-006).

  baseline.py --write    record tests/gc/baseline/bench2.txt and perf.txt with the current build
  baseline.py            compare the current build's bench2 reference outputs with bench2.txt
  baseline.py --stress   the same with build-stress/vo collecting every 64th allocation (SC-003)

bench2.txt lines: <task> <case> <exit> <sha256 of stdout>. perf.txt: the median of three
`tests/perf.sh` runs, one `<program> <total ms>` line each.
"""
import concurrent.futures as cf
import hashlib
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BASE = os.path.join(ROOT, "tests", "gc", "baseline")


def jobs():
    for group in ("tasks", "features"):
        gdir = os.path.join(ROOT, "bench2", group)
        for task in sorted(os.listdir(gdir)):
            tdir = os.path.join(gdir, task)
            if not os.path.isdir(os.path.join(tdir, "ref", "vo")):
                continue
            for kind in ("visible", "hidden"):
                cdir = os.path.join(tdir, kind)
                if os.path.isdir(cdir):
                    for n in sorted(x[:-3] for x in os.listdir(cdir) if x.endswith(".in")):
                        yield task, tdir, f"{kind}/{n}", os.path.join(cdir, n + ".in")


def run_case(job, env):
    task, tdir, case, inp = job
    with tempfile.TemporaryDirectory() as tmp:
        fx = os.path.join(tdir, "fixtures")
        if os.path.isdir(fx):
            shutil.copytree(fx, tmp, dirs_exist_ok=True)
        shutil.copytree(os.path.join(tdir, "ref", "vo"), tmp, dirs_exist_ok=True)
        cmd = [os.path.join(ROOT, "bin", "vo"), "run", "--timeout", "120", "main.vo"]
        r = subprocess.run(cmd, input=open(inp, "rb").read(), capture_output=True, cwd=tmp, env=env, timeout=300)
        if r.returncode == 124 and "VO_GC_STRESS" in env:
            # a large live heap is too slow to collect every 64 allocations: stress it less often
            env = dict(env, VO_GC_STRESS="4096")
            r = subprocess.run(cmd, input=open(inp, "rb").read(), capture_output=True, cwd=tmp, env=env, timeout=300)
    return f"{task} {case} {r.returncode} {hashlib.sha256(r.stdout).hexdigest()}"


def bench_lines(env):
    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        return list(ex.map(lambda j: run_case(j, env), list(jobs())))


def perf_lines():
    runs = []
    for _ in range(3):
        out = subprocess.run(["sh", os.path.join(ROOT, "tests", "perf.sh")], capture_output=True, text=True).stdout
        runs.append(re.findall(r"^(perf[^:]*):.*total (\d+) ms", out, re.M))
    names = [n for n, _ in runs[0]]
    return [f"{n.replace(' ', '_')} {int(statistics.median(int(r[i][1]) for r in runs))}" for i, n in enumerate(names)]


def main():
    args = sys.argv[1:]
    env = {k: os.environ[k] for k in ("PATH", "HOME", "LANG") if k in os.environ}
    if "--stress" in args:
        env["VO_BIN"] = os.path.join(ROOT, "build-stress", "vo")
        env["VO_GC_STRESS"] = os.environ.get("VO_GC_STRESS", "64")
    elif "VO_BIN" in os.environ:
        env["VO_BIN"] = os.environ["VO_BIN"]
    if "--write" in args:
        os.makedirs(BASE, exist_ok=True)
        open(os.path.join(BASE, "bench2.txt"), "w").write("\n".join(bench_lines(env)) + "\n")
        open(os.path.join(BASE, "perf.txt"), "w").write("\n".join(perf_lines()) + "\n")
        print("baseline written to", BASE)
        return 0
    want = open(os.path.join(BASE, "bench2.txt")).read().split("\n")[:-1]
    got = bench_lines(env)
    bad = [(w, g) for w, g in zip(want, got) if w != g]
    for w, g in bad[:20]:
        print(f"DIFF want {w}\n     got  {g}")
    print(f"baseline: {len(got) - len(bad)}/{len(got)} bench2 reference runs unchanged"
          + (" (stress)" if "--stress" in args else ""))
    return 1 if bad or len(want) != len(got) else 0


if __name__ == "__main__":
    sys.exit(main())
