#!/usr/bin/env python3
"""Spec 031 SC-005: a constant and its value written as a literal mean the same, at every position.

  const_equiv.py [--count N] [--seed S]

Generates N programs with literals in expressions, an effect scope and a target checked against it, a
field annotation, a format template and a regex pattern, and the same programs with every literal
replaced by a constant (half of them by a two-part constant expression), declared at the end of the
file. Some programs have a mistake (an invalid scope, a template with the wrong placeholder count, a
bad pattern, a target outside the scope). The two versions must give the same diagnostic codes from
`vo check --json` and, when they check clean, the same `vo run` output and status and the same
`vo effects` output. Diagnostic positions and wording are not compared: a constant's use is not at the
literal's column, and messages about its value name it.
"""
import argparse
import json
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")


def q(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def program(r, use_consts):
    consts = []

    def lit(text, value_kind, parts=None):
        """The literal text, or a constant standing for it (parts: two texts joined by + for the expression form)."""
        if not use_consts:
            return text
        name = f"c{len(consts)}"
        value = f"{parts[0]} + {parts[1]}" if parts and r.random() < 0.5 else text
        consts.append(f"const {name} = {value};")
        return name

    def int_lit(n):
        a = r.randrange(-50, 50)
        return lit(str(n), "int", (str(a), str(n - a)) if n - a >= 0 else None)

    def str_lit(s):
        k = r.randrange(len(s) + 1)
        return lit(q(s), "str", (q(s[:k]), q(s[k:])))

    a, b = r.randrange(-1000, 1000), r.randrange(1, 50)
    lim = r.randrange(1, 9)
    dirname = r.choice(["out/", "data/notes/", "./", "", "/etc/", "a/../b/"] if r.random() < 0.2 else ["out/", "data/notes/", "logs/"])
    target_dir = dirname if r.random() < 0.85 else "tmp/"
    holes = r.choice([2, 2, 2, 1, 3])
    template = " ".join(["{}"] * holes) + r.choice(["", " |", " {:>4}"[:0]])
    pattern = r.choice(["^[a-z]+[0-9]$", "^x+$", "[0-9]{2}"] + (["[a-z", "a{2,1}"] if r.random() < 0.1 else []))
    word = r.choice(["tea", "ab1", "x", "99", "hello world"])
    dir_c = str_lit(dirname)
    lines = [
        "import std.regex.is_match;",
        "import std.regex.regex;",
        "",
        "record Item {",
        f"    @size(1, {int_lit(lim)})",
        "    name: str;",
        "}",
        "",
        "fn compute() -> int {",
        f"    return {int_lit(a)} * {int_lit(b)} + ({int_lit(a)} - {int_lit(b)}) / {int_lit(b)};",
        "}",
        "",
        f"fn save() -> result[void] uses file.write({dir_c}) {{",
        f"    return write_file({str_lit(target_dir) if not use_consts or target_dir != dirname else dir_c} + {q('x.txt')}, {q('x')});",
        "}",
        "",
        "fn main() uses console {",
        f"    print(format({str_lit(template)}, compute(), {str_lit(word)}));",
        f"    print(str(is_match({str_lit(word)}, regex({str_lit(pattern)}))));",
        f"    print(str(new Item {{ name: {str_lit(word)} }}.name == {str_lit(word)}));",
        "}",
    ]
    return "\n".join(lines + [""] + consts) + "\n"


def vo(args, cwd):
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=60)
    return p.returncode, p.stdout, p.stderr


def outcome(src, d, name):
    path = os.path.join(d, name)
    open(path, "w").write(src)
    code, out, _ = vo(["check", "--json", name], d)
    codes = sorted(json.loads(l)["code"] for l in out.decode().splitlines() if '"kind":"diagnostic"' in l)
    if code != 0:
        return ("check", code, codes)
    run = vo(["run", name], d)
    eff = vo(["effects", name], d)
    return ("ran", codes, run[0], run[1], eff[1].replace(name.encode(), b"P"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=1000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    bad = clean = 0
    with tempfile.TemporaryDirectory() as d:
        for seed in range(args.seed, args.seed + args.count):
            a = outcome(program(random.Random(seed), False), d, "lit.vo")
            b = outcome(program(random.Random(seed), True), d, "con.vo")
            clean += a[0] == "ran"
            if a != b:
                bad += 1
                if bad <= 5:
                    print(f"DIFF seed {seed}:\n  literals:  {a}\n  constants: {b}")
                    open(os.path.join(ROOT, "build", f"const_equiv_{seed}_lit.vo"), "w").write(program(random.Random(seed), False))
                    open(os.path.join(ROOT, "build", f"const_equiv_{seed}_con.vo"), "w").write(program(random.Random(seed), True))
    print(f"const_equiv: {args.count} programs ({clean} check clean), {bad} differences")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
