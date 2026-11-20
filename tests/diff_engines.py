#!/usr/bin/env python3
"""Differential test of the two engines (spec 004, research R13, FR-015, SC-005).

  diff_engines.py --count N --seed S [--stress]

Runs N generated programs (tests/gen_programs.py, seeds S..S+N-1) on build-oracle/vo, the tree walker
before spec 004, and on build/vo (with --stress: build-stress/vo collecting every 64 allocations), and
compares stdout, stderr and exit status (floats at the oracle's 15 significant digits, tests/float15.py).
Disagreements go to build/diff-failures/<seed>/.
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import float15  # noqa: E402
import gen_programs  # noqa: E402

VO = os.path.join(ROOT, "bin", "vo")
WORK = os.path.join(ROOT, "build", "diff-work")
FAILS = os.path.join(ROOT, "build", "diff-failures")


def run(binary, path, extra_env):
    env = dict(os.environ, VO_BIN=binary, **extra_env)
    r = subprocess.run([VO, "run", "--timeout", "10", path], capture_output=True, env=env, timeout=120)
    err = r.stderr.replace(os.path.dirname(path).encode() + b"/", b"")
    # spec 006: runtime errors list each frame's values; the oracle (spec 003) does not
    err = re.sub(rb"(?m)^      \S.*\n", b"", err)
    return r.stdout, err, r.returncode


def one(seed, new_bin, new_env):
    src = gen_programs.generate(seed)
    path = os.path.join(WORK, f"p{seed}.vo")
    open(path, "w").write(src)
    a = run(os.path.join(ROOT, "build-oracle", "vo"), path, {})
    b = run(new_bin, path, new_env)
    os.remove(path)
    if b"error[E" in a[1]:
        return "unchecked", seed, src, a, b
    if b"error[R0009]" in a[1] or b"error[R0009]" in b[1]:
        return "timeout", seed, src, a, b
    if not float15.same(a, b):  # the oracle writes floats to 15 digits
        return "diff", seed, src, a, b
    return ("runtime" if b"runtime error[" in a[1] else "ok"), seed, src, a, b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=1000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--stress", action="store_true")
    args = ap.parse_args()
    if not os.path.exists(os.path.join(ROOT, "build-oracle", "vo")):
        print("diff_engines: build-oracle/vo is missing; run `make oracle`")
        return 1
    if args.stress:
        new_bin, new_env = os.path.join(ROOT, "build-stress", "vo"), {"VO_GC_STRESS": "64"}
    else:
        new_bin, new_env = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo")), {}
    os.makedirs(WORK, exist_ok=True)
    counts = {"ok": 0, "runtime": 0, "diff": 0, "unchecked": 0, "timeout": 0}
    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        futs = [ex.submit(one, s, new_bin, new_env) for s in range(args.seed, args.seed + args.count)]
        for f in cf.as_completed(futs):
            kind, seed, src, a, b = f.result()
            counts[kind] += 1
            if kind in ("diff", "unchecked", "timeout"):
                d = os.path.join(FAILS, f"{seed}-{kind}")
                os.makedirs(d, exist_ok=True)
                open(os.path.join(d, "p.vo"), "w").write(src)
                for tag, (out, err, code) in (("oracle", a), ("new", b)):
                    open(os.path.join(d, tag + ".out"), "wb").write(out)
                    open(os.path.join(d, tag + ".err"), "wb").write(err + f"\nexit {code}\n".encode())
                if kind == "diff" and counts["diff"] <= 5:
                    print(f"DIFF seed {seed}: see {d}")
    n = args.count
    print(f"diff_engines: {n} programs, {counts['diff']} disagreements, {counts['runtime']} stopped on a runtime "
          f"error, {counts['unchecked']} did not check, {counts['timeout']} timed out"
          + (" (stress)" if args.stress else ""))
    return 1 if counts["diff"] or counts["unchecked"] > n // 100 or counts["timeout"] else 0


if __name__ == "__main__":
    sys.exit(main())
