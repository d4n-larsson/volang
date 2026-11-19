#!/usr/bin/env python3
"""Spec 024 SC-001: `format` against Python's format() (research R7).

Generates vo programs of 5,000 lines, `print(format("{:SPEC}", EXPR));`, runs them and compares
every line with Python's format(value, SPEC). Values are built from expressions both languages
evaluate identically under IEEE 754 (vo float literals have no exponent and at most 15 digits):
int literals; float(a) / float(b) with |a|, |b| < 2^53; chains of * 10.0 and / 10.0; known ties;
halving and doubling loops for subnormals and huge values. Specs: every combination the contract
allows (contracts/language.md), floats only with a precision.

  format_diff.py [--quick] [--lines N] [--cross BIN...]

--quick runs 5,000 lines (default 100,000). --cross runs the same programs with each build
(through bin/vo, VO_BIN=BIN: qemu-user for other architectures) and requires identical output.
Exit 1 on any difference; the first one is printed with its value and spec.
"""
import math
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CHUNK = 5000
HELPERS = """fn halve(x: float, n: int) -> float {
    var v = x;
    for i in 0..n {
        set v = v / 2.0;
    }
    return v;
}

fn twice(x: float, n: int) -> float {
    var v = x;
    for i in 0..n {
        set v = v * 2.0;
    }
    return v;
}

"""


def int_value(rng):
    pick = rng.random()
    if pick < 0.1:
        v = rng.choice([0, 1, -1, 2**63 - 1, -2**63, 999, 1000, -1000, 255, -255])
    elif pick < 0.5:
        v = rng.randint(-100000, 100000)
    else:
        v = rng.randint(-2**63, 2**63 - 1)
    text = "-9223372036854775807 - 1" if v == -2**63 else str(v)
    return text, v


def float_value(rng):
    pick = rng.random()
    if pick < 0.1:
        lit = rng.choice(["0.125", "2.5", "2.675", "1.005", "0.5", "1.5", "0.00001", "99.995", "-0.0", "0.0"])
        return lit, float(lit)
    if pick < 0.2:
        n = rng.randint(1, 1074)
        start = rng.choice(["1.0", "3.0", "-7.0"])
        v = float(start)
        for _ in range(n):
            v = v / 2.0
        return f"halve({start}, {n})", v
    if pick < 0.25:
        n = rng.randint(1, 1023)
        start = rng.choice(["1.0", "1.5", "-1.75"])
        v = float(start)
        for _ in range(n):
            v = v * 2.0
        return f"twice({start}, {n})", v
    a = rng.randint(-2**53 + 1, 2**53 - 1) if rng.random() < 0.5 else rng.randint(-100000, 100000)
    b = rng.randint(1, 2**53 - 1) if rng.random() < 0.5 else rng.randint(1, 1000)
    text = f"float({a}) / float({b})"
    v = float(a) / float(b)
    k = rng.choice([0, 0, 0, 1, 2, 5, 10, 20])
    op = rng.choice(["*", "/"])
    for _ in range(k):
        text += f" {op} 10.0"
        v = v * 10.0 if op == "*" else v / 10.0
    return text, v


def spec_for(rng, kind):
    align = rng.choice(["", "", "<", ">", "^"])
    plus = rng.choice(["", "+"])
    zero = rng.choice(["", "0"]) if not align else ""
    width = rng.choice([""] + [str(w) for w in range(1, 13)])
    radix = rng.choice(["", "", "x", "b"]) if kind == "int" else ""
    group = rng.choice(["", ","]) if not zero and not radix else ""
    prec = f".{rng.randint(0, 10)}f" if kind == "float" else ""
    return align + plus + zero + width + group + prec + radix


def cases(rng, n):
    out = []
    while len(out) < n:
        kind = "int" if rng.random() < 0.4 else "float"
        text, v = int_value(rng) if kind == "int" else float_value(rng)
        if kind == "float" and not math.isfinite(v):
            continue
        spec = spec_for(rng, kind)
        out.append((text, v, spec, format(v, spec)))
    return out


def run(vo_env, path):
    p = subprocess.run([VO, "run", path], capture_output=True, env=vo_env, timeout=600)
    if p.returncode != 0:
        sys.exit(f"format_diff: {path} exited {p.returncode}: {p.stderr.decode(errors='replace')[:2000]}")
    return p.stdout.decode().split("\n")[:-1]


def main():
    args = sys.argv[1:]
    lines = 100000
    cross = []
    if "--quick" in args:
        lines = CHUNK
    if "--lines" in args:
        lines = int(args[args.index("--lines") + 1])
    if "--cross" in args:
        cross = args[args.index("--cross") + 1:]
    rng = random.Random(1)
    all_cases = cases(rng, lines)
    tmp = tempfile.mkdtemp(prefix="vofmt-")
    fails = 0
    try:
        for c0 in range(0, len(all_cases), CHUNK):
            chunk = all_cases[c0:c0 + CHUNK]
            path = os.path.join(tmp, f"p{c0 // CHUNK}.vo")
            with open(path, "w") as f:
                f.write(HELPERS + "fn main() uses console {\n")
                for text, _, spec, _ in chunk:
                    f.write(f'    print(format("{{:{spec}}}", {text}));\n')
                f.write("}\n")
            got = run(dict(os.environ), path)
            for (text, v, spec, want), line in zip(chunk, got + [None] * (len(chunk) - len(got))):
                if line != want:
                    fails += 1
                    if fails == 1:
                        print(f"FAIL format(\"{{:{spec}}}\", {text}) (value {v!r}): vo {line!r}, Python {want!r}")
            for b in cross:
                other = run(dict(os.environ, VO_BIN=b), path)
                if other != got:
                    fails += 1
                    print(f"FAIL {b} differs from {VO} on {path}")
    finally:
        for n in os.listdir(tmp):
            os.remove(os.path.join(tmp, n))
        os.rmdir(tmp)
    print(f"format_diff: {len(all_cases)} values, {fails} differences" + (f", {len(cross)} builds compared" if cross else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
