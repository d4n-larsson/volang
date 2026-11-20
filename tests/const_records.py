#!/usr/bin/env python3
"""Spec 031 SC-001, SC-003: constants in the examples change nothing in review.

For every example that declares a constant, the example as it is and the same example with each
constant's uses replaced by its value (and the declarations removed) must give byte-identical `vo
effects`, `vo check` and `vo test` output (test timings aside). Then, in a copy with a `vo.effects`
record, changing a directory constant to "./" must make `vo check` report E0504 for the functions whose
scope it is.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CONST = re.compile(r'^(?:export )?const ([a-z][a-z0-9_]*) = ("(?:[^"\\]|\\.)*"|-?[0-9.]+|true|false|[a-z][a-z0-9_]*)'
                   r'( \+ ("(?:[^"\\]|\\.)*"|[a-z][a-z0-9_]*))?;\n', re.M)


def inline(text):
    """The text with its simple constants written out as literals (a value, or NAME + "lit")."""
    values = {}
    for m in CONST.finditer(text):
        name, first, _, second = m.groups()
        if first[0].isalpha() and first not in ("true", "false"):
            if first not in values:
                continue
            first = values[first]
        if second:
            second = values.get(second, second)
            if first.startswith('"') and second.startswith('"'):
                first = first[:-1] + second[1:]
            else:
                continue
        values[name] = first
    out = CONST.sub(lambda m: "" if m.group(1) in values else m.group(0), text)
    for name, value in values.items():
        out = re.sub(r"(?<![\w.\"])" + name + r"(?![\w\"(])", value.replace("\\", "\\\\"), out)
    # a literal joined to a literal ("out/" + "x") is the same value as one literal
    out = re.sub(r'"((?:[^"\\]|\\.)*)" \+ "((?:[^"\\]|\\.)*)"', r'"\1\2"', out)
    return out, values


def run(args, cwd):
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=120)
    out = re.sub(rb"\(\d+ms\)|\(\d+ cases?, \d+ms\)", b"(T)", p.stdout + p.stderr)
    return p.returncode, out


def outcome(d):
    return [run(["effects", "main.vo"], d), run(["check", "main.vo"], d), run(["test", "main.vo"], d)]


def main():
    problems = examples = 0
    for name in sorted(os.listdir(os.path.join(ROOT, "examples"))):
        src = os.path.join(ROOT, "examples", name)
        if not os.path.exists(os.path.join(src, "main.vo")):
            continue
        files = [f for f in os.listdir(src) if f.endswith(".vo")]
        if not any(CONST.search(open(os.path.join(src, f)).read()) for f in files):
            continue
        examples += 1
        with tempfile.TemporaryDirectory() as t:
            a, b = os.path.join(t, "a"), os.path.join(t, "b")
            shutil.copytree(src, a)
            shutil.copytree(src, b)
            dirs = {}
            for f in files:
                text, values = inline(open(os.path.join(b, f)).read())
                open(os.path.join(b, f), "w").write(text)
                dirs.update({k: v for k, v in values.items() if v.endswith('/"')})
            if outcome(a) != outcome(b):
                problems += 1
                print(f"FAIL {name}: constants and their values give different reports")
                for x, y in zip(outcome(a), outcome(b)):
                    if x != y:
                        print("  with constants:", x[1][:300])
                        print("  with values:   ", y[1][:300])
                continue
            run(["effects", "--update", "main.vo"], a)
            for f in files:
                p = os.path.join(a, f)
                text = open(p).read()
                for const in dirs:
                    text = re.sub(r"^((?:export )?const " + const + r" = )\"[^\"]*\";", r'\1"./";', text, flags=re.M)
                open(p, "w").write(text)
            code, out = run(["check", "main.vo"], a)
            if dirs and b"E0504" not in out:
                problems += 1
                print(f"FAIL {name}: changing {', '.join(dirs)} to \"./\" is not reported (E0504)")
                continue
            print(f"ok   {name}: {len(dirs)} directory constant(s); same reports; a change is E0504")
    print(f"const_records: {examples} examples with constants, {problems} problems")
    return 1 if problems or not examples else 0


if __name__ == "__main__":
    sys.exit(main())
