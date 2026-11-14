#!/usr/bin/env python3
"""Spec 006: malformed query targets, diffs and recordings never crash the toolchain; spec 007:
malformed JSON never crashes std.json (tests/json/driver.vo).

Starts from valid inputs (tests/cases/query/shop, tests/cases/query/impact/*.diff, a recording
made here and the tests/json corpus), then truncates them and flips, inserts and deletes bytes at random. Every run must end
with a documented exit status (0, 1, 64, 65, 66, 70, 74, 124) and never with 125 or a signal
(Principle III).

  fuzz_inputs.py [--iterations N] [--seed N]
"""
import os
import random
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
OK = {0, 1, 64, 65, 66, 70, 74, 124}


def mutate(rng, data):
    data = bytearray(data)
    for _ in range(rng.randint(1, 4)):
        op = rng.randrange(4)
        if op == 0 and data:
            del data[rng.randrange(len(data)):]
        elif op == 1 and data:
            data[rng.randrange(len(data))] = rng.randrange(256)
        elif op == 2:
            data.insert(rng.randrange(len(data) + 1), rng.choice(b'{}[]":,\\\n -+@0123456789abu'))
        elif data:
            del data[rng.randrange(len(data))]
    return bytes(data)


def run(args, cwd, stdin=b""):
    r = subprocess.run([VO] + args, cwd=cwd, input=stdin, capture_output=True, timeout=60)
    return r.returncode


def main():
    iterations, seed = 300, 1
    args = sys.argv[1:]
    if "--iterations" in args:
        iterations = int(args[args.index("--iterations") + 1])
    if "--seed" in args:
        seed = int(args[args.index("--seed") + 1])
    rng = random.Random(seed)
    work = tempfile.mkdtemp()
    problems = []
    try:
        shutil.copytree(os.path.join(ROOT, "tests", "cases", "query", "impact", "shop"), os.path.join(work, "shop"))
        diffs = []
        idir = os.path.join(ROOT, "tests", "cases", "query", "impact")
        for n in sorted(os.listdir(idir)):
            if n.endswith(".diff"):
                diffs.append(open(os.path.join(idir, n), "rb").read())
        sd = os.path.join(work, "shop")
        open(os.path.join(sd, "in.txt"), "w").write("3\n")
        rec_prog = ('fn main() uses console, input, env, clock {\n    match read_line() {\n'
                    '        case ok(l) {\n            print(l);\n        }\n        case err(e) {\n'
                    '            print(e);\n        }\n    }\n    print(str(now_ms() - now_ms()));\n'
                    '    match env("HOME") {\n        case ok(h) {\n            print("home");\n        }\n'
                    '        case err(e) {\n            print(e);\n        }\n    }\n}\n')
        open(os.path.join(work, "rec.vo"), "w").write(rec_prog)
        subprocess.run([VO, "run", "--seed", "1", "--record", "r.jsonl", "rec.vo"], cwd=work,
                       input=b"line\n", capture_output=True, timeout=60)
        recording = open(os.path.join(work, "r.jsonl"), "rb").read()
        jdir = os.path.join(ROOT, "tests", "json")
        docs = [open(os.path.join(jdir, k, n), "rb").read() for k in ("valid", "invalid")
                for n in sorted(os.listdir(os.path.join(jdir, k))) if n.endswith(".json")]
        targets = [b"checkout", b"tax", b"pricing.vo:1:4", b"cart.vo:3:13", b"main.vo:10:1"]
        for i in range(iterations):
            kind = i % 4
            if kind == 0:
                t = mutate(rng, rng.choice(targets)).decode("utf-8", "replace").replace("\x00", "")
                t = t or "x"
                cmd = rng.choice([["query", "refs", "shop/main.vo", t], ["query", "callers", "shop/main.vo", t],
                                  ["query", "type-at", "shop/main.vo", "shop/" + t],
                                  ["edit", "rename", "--dry-run", "shop/main.vo", t, "renamed_x"]])
                code = run(cmd, work)
                what = "%r" % cmd
            elif kind == 1:
                d = mutate(rng, rng.choice(diffs))
                open(os.path.join(work, "d.diff"), "wb").write(d)
                code = run(["impact", "main.vo", "--diff", "../d.diff"], sd)
                what = "impact with diff %r" % d[:120]
            elif kind == 3:
                j = mutate(rng, rng.choice(docs))
                r = subprocess.run([VO, "run", os.path.join(jdir, "driver.vo")], input=j,
                                   capture_output=True, timeout=60)
                code = r.returncode if b"round trip changed" not in r.stdout else 125
                what = "json driver on %r" % j[:160]
            else:
                r = mutate(rng, recording)
                open(os.path.join(work, "m.jsonl"), "wb").write(r)
                code = run(["replay", "--timeout", "5", "m.jsonl"], work)
                what = "replay of %r" % r[:160]
            if code not in OK:
                problems.append("exit %d: %s" % (code, what))
    finally:
        shutil.rmtree(work)
    for p in problems[:20]:
        print("FAIL", p)
    print("fuzz_inputs: %d runs, %d problems" % (iterations, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
