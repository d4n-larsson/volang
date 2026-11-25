#!/usr/bin/env python3
"""Spec 030 SC-003: naming repeated effects in a set changes no record and no behavior.

For each example and for generated programs, every `uses` clause of an item header that appears in two or
more headers of a file is replaced by a set (`effects Set1 = ...;`, `uses Set1`). The rewritten program
must report the same diagnostics, its `vo.effects` (from `vo effects --update`) must be byte-identical to the original's,
and its tests (examples) or its output (generated programs) must be the same.

  effect_sets_records.py [--count N] [--seed S]
"""
import argparse
import glob
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
# an item header's clause: `uses ...` up to ` {` at the end of the line (lower-case effects only)
CLAUSE = re.compile(r'^((?:export )?(?:fn|test) .*?|    )uses ((?:[a-z.]+(?:\("[^"]*"(?:, "[^"]*")*\))?(?:, )?)+) \{$', re.M)


def vo(args, cwd):
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout + p.stderr


def rewrite(path):
    """Replaces repeated header clauses by sets; returns how many sets were added."""
    text = open(path).read()
    counts = {}
    for m in CLAUSE.finditer(text):
        counts[m.group(2)] = counts.get(m.group(2), 0) + 1
    repeated = sorted(c for c, n in counts.items() if n >= 2)
    if not repeated:
        return 0
    names = {c: f"Set{i + 1}" for i, c in enumerate(repeated)}
    text = CLAUSE.sub(lambda m: f"{m.group(1)}uses {names[m.group(2)]} {{" if m.group(2) in names else m.group(0), text)
    decls = "".join(f"effects {names[c]} = {c};\n" for c in repeated)
    lines = text.split("\n")
    at = max([i + 1 for i, l in enumerate(lines) if l.startswith("import ")] or [0])
    lines.insert(at, "\n" + decls)
    open(path, "w").write("\n".join(lines))
    return len(repeated)


def record(d, entry):
    if os.path.exists(os.path.join(d, "vo.effects")):
        os.remove(os.path.join(d, "vo.effects"))
    code, out = vo(["effects", "--update", entry], d)
    if code != 0:
        return None, out
    return open(os.path.join(d, "vo.effects")).read(), out


def compare(src_dir, entry, run_args, problems, label):
    with tempfile.TemporaryDirectory() as tmp:
        a, b = os.path.join(tmp, "a"), os.path.join(tmp, "b")
        shutil.copytree(src_dir, a, ignore=shutil.ignore_patterns("data", "out", "recordings"))
        shutil.copytree(a, b)
        added = sum(rewrite(f) for f in glob.glob(os.path.join(b, "**", "*.vo"), recursive=True))
        if not added:
            return 0
        vo(["fmt", "."], b)
        code, out = vo(["check", entry], b)
        codes = lambda o: sorted(re.findall(r"(?:error|warning)\[(\w+)\]", o))
        if code != 0 or codes(out) != codes(vo(["check", entry], a)[1]):
            problems.append(f"{label}: the rewritten program reports other diagnostics:\n{out}")
            return added
        ra, out_a = record(a, entry)
        rb, out_b = record(b, entry)
        if ra is None or ra != rb:
            problems.append(f"{label}: vo.effects differs:\n--- original\n{ra or out_a}--- with sets\n{rb or out_b}")
        for d in (a, b):
            for sub in ("data", "out"):
                os.makedirs(os.path.join(d, sub), exist_ok=True)
        ms = lambda r: (r[0], re.sub(r"\b\d+ms\)", "0ms)", r[1]))
        oa, ob = ms(vo(run_args + [entry], a)), ms(vo(run_args + [entry], b))
        if oa != ob:
            problems.append(f"{label}: `vo {' '.join(run_args)}` differs:\n--- original\n{oa[1]}--- with sets\n{ob[1]}")
        return added


POOL = ['console', 'file.read("out/")', 'file.write("out/")', 'clock', 'random', 'env("HOME")']
USE = {
    'console': '    print("f{i}");\n',
    'file.read("out/")': '    let r{i} = read_file("out/f{i}.txt");\n    let _ = r{i} == ok("");\n',
    'file.write("out/")': '    try write_file("out/f{i}.txt", "x");\n',
    'clock': '    set n = n + now_ms();\n',
    'random': '    set n = n + random_int(1, 6);\n',
    'env("HOME")': '    let _ = env("HOME") == ok("");\n',
}
ORDER = POOL  # canonical order of the pool's effects


def generate(rng):
    clauses = [sorted(rng.sample(POOL, rng.randint(1, 3)), key=ORDER.index) for _ in range(rng.randint(2, 4))]
    fns, calls, used = [], [], set()
    for i in range(rng.randint(4, 9)):
        c = rng.choice(clauses)
        used.update(c)
        body = "    var n = 0;\n" + "".join(USE[e].format(i=i) for e in c) + "    return ok(n);\n"
        fns.append(f"fn f{i}() -> result[int] uses {', '.join(c)} {{\n{body}}}\n")
        calls.append(f"    print(str(try f{i}()));\n")
    main_uses = sorted(used | {"console"}, key=ORDER.index)
    return "\n".join(fns) + f"\nfn main() -> result[void] uses {', '.join(main_uses)} {{\n" + "".join(calls) + \
        "    return ok();\n}\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=20)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    problems, sets = [], 0
    for m in sorted(glob.glob(os.path.join(ROOT, "examples", "*", "main.vo")) + glob.glob(os.path.join(ROOT, "examples", "*", "*", "main.vo"))):
        d = os.path.dirname(m)
        sets += compare(d, "main.vo", ["test"], problems, os.path.relpath(d, ROOT))
    rng = random.Random(args.seed)
    for k in range(args.count):
        with tempfile.TemporaryDirectory() as src:
            open(os.path.join(src, "main.vo"), "w").write(generate(rng))
            added = compare(src, "main.vo", ["run", "--seed", "1", "--clock-ms", "5"], problems, f"generated {k}")
            if not added:
                problems.append(f"generated {k}: no repeated clause (generator)")
            sets += added
    for p in problems:
        print("FAIL", p)
    print(f"effect_sets_records: {len(problems)} problems, {sets} sets introduced")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
