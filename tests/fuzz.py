#!/usr/bin/env python3
"""Malformed-input fuzzer (SC-007): no crash, no hang, always structured output.

Usage: python3 tests/fuzz.py --iterations N --seed S
"""
import argparse
import json
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
WORK = os.path.join(ROOT, "build", "fuzz")
FAILS = os.path.join(ROOT, "build", "fuzz-failures")
ALLOWED = set(range(0, 64)) | {65, 70, 124}


def corpus():
    files = []
    for d, _, names in os.walk(os.path.join(ROOT, "tests", "cases")):
        files += [os.path.join(d, n) for n in names if n.endswith(".vo")]
    return sorted(files)


def mutate(data, rng):
    data = bytearray(data)
    for _ in range(rng.randint(1, 5)):
        op = rng.randrange(6)
        n = len(data)
        if op == 0 and n:  # delete bytes
            i = rng.randrange(n)
            del data[i:i + rng.randint(1, 3)]
        elif op == 1 and n:  # duplicate a token-sized chunk
            i = rng.randrange(n)
            data[i:i] = data[i:i + rng.randint(1, 8)]
        elif op == 2 and n > 1:  # swap two bytes
            i, j = rng.randrange(n), rng.randrange(n)
            data[i], data[j] = data[j], data[i]
        elif op == 3 and n:  # delete a token-sized chunk
            i = rng.randrange(n)
            del data[i:i + rng.randint(1, 8)]
        elif op == 4:  # insert random bytes, including invalid UTF-8
            i = rng.randrange(n + 1)
            data[i:i] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 4)))
        else:  # insert syntax-shaped noise
            i = rng.randrange(n + 1)
            data[i:i] = rng.choice([b"{", b"}", b"(", b")", b";", b",", b"\"", b"\n",
                                    b"let ", b"fn ", b"match ", b"case ", b"[", b"]", b"..", b"#",
                                    # v2: effects, functions as values, generics
                                    b" uses ", b"console", b"file.write(\"out/\")", b"env(\"X\")",
                                    b"fn(", b"-> ", b"fn(x: int) { return x; }", b")(", b"[T]",
                                    b"[T: ordered]", b": effects", b"sort_by(", b"map(", b"new Box[int] ",
                                    b"uses E", b"Stack[", b"fold(",
                                    # spec 029: field annotations, records without fields
                                    b"@size(1, 2)\n", b"@range(-1.5, ", b"@default(", b"@default([])\n",
                                    b"@pattern(\"[a-z\")\n", b"@header(\"X\")", b"@", b"new P {}", b"{}",
                                    # spec 030: effect sets
                                    b"\neffects S = ", b"\neffects S = file.read(\"x/\");\n", b"export effects ",
                                    b"uses Storage", b"uses console, Storage", b"effects ", b"Storage"])
    return bytes(data)


ENV = None       # the inherited environment, unless --stress
SLOW = 1


def run(args, path):
    try:
        p = subprocess.run([VO] + args + [path], capture_output=True, timeout=5 * SLOW,
                           stdin=subprocess.DEVNULL, cwd=WORK, env=ENV)
    except subprocess.TimeoutExpired:
        return None, b"", b""
    return p.returncode, p.stdout, p.stderr


def problem(args, path):
    code, out, err = run(args, path)
    if code is None:
        return "hung (killed after 5 s)"
    if code not in ALLOWED:
        return f"exit status {code}"
    if args[0] == "check":
        for line in out.decode("utf-8", "replace").splitlines():
            try:
                rec = json.loads(line)
            except ValueError:
                return f"stdout line is not JSON: {line[:120]!r}"
            if rec.get("v") != 1:
                return f"record without \"v\":1: {line[:120]!r}"
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--stress", action="store_true", help="run with build-stress/vo, collecting every 16th allocation")
    a = ap.parse_args()
    global ENV, SLOW
    if a.stress:
        ENV = dict(os.environ, VO_BIN=os.path.join(ROOT, "build-stress", "vo"), VO_GC_STRESS="16")
        SLOW = 4
    rng = random.Random(a.seed)
    os.makedirs(WORK, exist_ok=True)
    files = corpus()
    if a.stress:  # the cases tests/run.sh --stress skips (`<case>.stress-every` is `skip`) do not fit that build
        def skipped(f):
            mark = f[:-3] + ".stress-every"
            return os.path.exists(mark) and open(mark).read().strip() == "skip"
        files = [f for f in files if not skipped(f)]
    if not files:
        print("fuzz: empty corpus")
        return 0
    hello = os.path.join(ROOT, "tests", "cases", "run", "hello.vo")
    run_ready = os.path.exists(hello) and run(["run"], hello)[0] != 125
    commands = [["check", "--json"]] + ([["run", "--timeout", "2"]] if run_ready else [])
    failures = 0
    for i in range(a.iterations):
        src = rng.choice(files)
        with open(src, "rb") as f:
            data = mutate(f.read(), rng)
        path = os.path.join(WORK, "case.vo")
        with open(path, "wb") as f:
            f.write(data)
        for args in commands:
            why = problem(args, path)
            if why:
                failures += 1
                os.makedirs(FAILS, exist_ok=True)
                keep = os.path.join(FAILS, f"seed{a.seed}-{i}.vo")
                with open(keep, "wb") as f:
                    f.write(data)
                print(f"FAIL vo {' '.join(args)} {keep}: {why} (from {os.path.relpath(src, ROOT)})")
    print(f"fuzz: {a.iterations} iterations, commands {[c[0] for c in commands]}, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
