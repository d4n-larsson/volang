#!/usr/bin/env python3
"""Differential test of the C port against the last assembly release (spec 008 research R12, FR-007).

  diff_asm.py --count N --seed S [--new PATH] [--platform x86_64|aarch64|riscv64] [--mutants M]
              [--mode mixed|float|intedge]

Runs N generated programs (tests/gen_programs.py, seeds S..S+N-1) on build-asm/vo (under qemu) and
on the build under test (default build/vo; --platform picks build-<arch>/vo, or build/vo for the host), and compares stdout,
stderr and exit status byte for byte. bin/vo runs each binary natively or under qemu by its
architecture. Disagreements go to build/diff-failures/asm-<seed>/; exit status 1 if there is any.

--mutants M also mutates M corpus sources (tests/fuzz.py's mutations: deleted, swapped and
inserted bytes, invalid UTF-8, syntax noise) and compares `vo check --json` and `vo run` on them:
the front end's diagnostics must match byte for byte as well.
"""
import argparse
import concurrent.futures as cf
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import float15  # noqa: E402
import gen_programs  # noqa: E402
import fuzz  # noqa: E402

VO = os.path.join(ROOT, "bin", "vo")
ASM = os.path.join(ROOT, "build-asm", "vo")
WORK = os.path.join(ROOT, "build", "diff-asm-work")
FAILS = os.path.join(ROOT, "build", "diff-failures")


def run(binary, path):
    env = dict(os.environ, VO_BIN=binary)
    r = subprocess.run([VO, "run", "--timeout", "10", path], capture_output=True, env=env, timeout=120, cwd=WORK)
    return r.stdout, r.stderr.replace(os.path.dirname(path).encode() + b"/", b""), r.returncode


def one(seed, new_bin, mode):
    src = gen_programs.generate(seed, mode)
    path = os.path.join(WORK, f"p{seed}.vo")
    open(path, "w").write(src)
    a, b = run(ASM, path), run(new_bin, path)
    os.remove(path)
    if b"error[R0009]" in a[1] or b"error[R0009]" in b[1]:
        return "timeout", seed, src, a, b
    return ("ok" if float15.same(a, b) else "diff"), seed, src, a, b  # the release writes floats to 15 digits


def run_cmd(binary, args, cwd):
    """In the mutant's own directory: a mutated program that writes a file (run/files.vo writes
    out.txt) must not write into the repository."""
    env = dict(os.environ, VO_BIN=binary)
    r = subprocess.run([VO] + args, capture_output=True, env=env, timeout=120, cwd=cwd)
    return r.stdout, r.stderr, r.returncode


def mutant(i, seed, new_bin, files):
    import random
    rng = random.Random(seed * 1000003 + i)
    src = fuzz.mutate(open(rng.choice(files), "rb").read(), rng)
    d = os.path.join(WORK, f"m{i}")  # its own directory: E0204 scans the directory for other files
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "m.vo")
    open(path, "wb").write(src)
    results = []
    for args in (["check", "--json", path], ["run", "--seed", "1", "--clock-ms", "0", "--timeout", "5", path]):
        a, b = run_cmd(ASM, args, d), run_cmd(new_bin, args, d)
        if b"error[R0009]" in a[1] or b"error[R0009]" in b[1]:
            continue
        results.append((args[0], a, b))
    shutil.rmtree(d)
    for cmd, a, b in results:
        if a != b:
            return "diff", i, src, a, b
    return "ok", i, src, None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--new", default=None)
    ap.add_argument("--platform", choices=["x86_64", "aarch64", "riscv64"], default=None)
    ap.add_argument("--mutants", type=int, default=0)
    ap.add_argument("--mode", choices=["mixed", "float", "intedge"], default="mixed")
    args = ap.parse_args()
    if not os.path.exists(ASM):
        print("diff_asm: build-asm/vo is missing; run `make asm`")
        return 1
    host = args.platform in (None, os.uname().machine)  # the host build is build/vo
    new_bin = args.new or os.path.join(ROOT, "build" if host else f"build-{args.platform}", "vo")
    os.makedirs(WORK, exist_ok=True)
    counts = {"ok": 0, "diff": 0, "timeout": 0}
    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        futs = [ex.submit(one, s, new_bin, args.mode) for s in range(args.seed, args.seed + args.count)]
        for f in cf.as_completed(futs):
            kind, seed, src, a, b = f.result()
            counts[kind] += 1
            if kind != "ok":
                d = os.path.join(FAILS, f"asm-{args.mode}-{seed}-{kind}")
                os.makedirs(d, exist_ok=True)
                open(os.path.join(d, "p.vo"), "w").write(src)
                for tag, (out, err, code) in (("asm", a), ("new", b)):
                    open(os.path.join(d, tag + ".out"), "wb").write(out)
                    open(os.path.join(d, tag + ".err"), "wb").write(err + f"\nexit {code}\n".encode())
                if counts[kind] <= 5:
                    print(f"{kind.upper()} seed {seed}: see {d}")
    mdiff = 0
    if args.mutants:
        # Spec 007: networking is a stub in build-asm/vo (no host check, always err), so sources that
        # use std.http are left out: their difference is the feature, not a porting error.
        files = [f for f in fuzz.corpus() if b"std.http" not in open(f, "rb").read()]
        with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            futs = [ex.submit(mutant, i, args.seed, new_bin, files) for i in range(args.mutants)]
            for f in cf.as_completed(futs):
                kind, i, src, a, b = f.result()
                if kind == "diff":
                    mdiff += 1
                    d = os.path.join(FAILS, f"asm-mutant-{args.seed}-{i}")
                    os.makedirs(d, exist_ok=True)
                    open(os.path.join(d, "m.vo"), "wb").write(src)
                    for tag, (out, err, code) in (("asm", a), ("new", b)):
                        open(os.path.join(d, tag + ".out"), "wb").write(out)
                        open(os.path.join(d, tag + ".err"), "wb").write(err + f"\nexit {code}\n".encode())
                    if mdiff <= 5:
                        print(f"DIFF mutant {i}: see {d}")
    print(f"diff_asm: {args.count} programs ({args.mode}), {counts['diff']} disagreements, {counts['timeout']} timed out"
          + (f"; {args.mutants} mutated sources, {mdiff} disagreements" if args.mutants else "")
          + f" ({os.path.relpath(new_bin, ROOT)} vs build-asm/vo)")
    return 1 if counts["diff"] or counts["timeout"] or mdiff else 0


if __name__ == "__main__":
    sys.exit(main())
