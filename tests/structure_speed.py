#!/usr/bin/env python3
"""Spec 027 SC-004: the structure rules make `vo check` at most 10% slower, on the largest example
program and on a generated, well-structured 5,000-line program (13 modules, 400 functions with loops and
branches, no warnings, so only the analysis is measured). The fastest of 21 interleaved runs each, with the rules and with VO_STRUCTURE_OFF=1 (a maintainers' switch that
skips them). Needs an idle machine: in `make gate`, not `make test`.
"""
import os
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")


def generated(d):
    """A well-structured program of about 5,000 lines: 13 modules of 30 functions each (under 400 code
    lines), and a thin main. Returns the entry path."""
    imports, calls = [], []
    for m in range(13):
        fns = []
        for i in range(30):
            fns.append(f"""fn step{i}(x: int, xs: list[int]) -> int {{
    var total = x;
    for v in xs {{
        if v > {i % 7} {{
            set total = total + v * {(i + m) % 5 + 1};
        }} else {{
            set total = total - {m + 1};
        }}
    }}
    return total + len(xs);
}}
""")
        run = "export fn run{m}(x: int) -> int {{\n    var t = x;\n".format(m=m) + "".join(
            f"    set t = step{(i * (2 * m + 1)) % 30}(t, [1, 2, {m}]);\n" for i in range(30)) + "    return t;\n}\n"
        with open(os.path.join(d, f"mod{m}.vo"), "w") as f:
            f.write("\n".join(fns) + "\n" + run)
        imports.append(f"import mod{m}.run{m};\n")
        calls.append(f"    set t = run{m}(t);\n")
    entry = os.path.join(d, "main.vo")
    with open(entry, "w") as f:
        f.write("".join(imports) + "\nfn main() uses console {\n    var t = 0;\n" + "".join(calls) + "    print(str(t));\n}\n")
    return entry


def once(path, env):
    t = time.monotonic()
    subprocess.run([VO, "check", os.path.basename(path)], cwd=os.path.dirname(path), capture_output=True,
                   timeout=120, env=env)
    return time.monotonic() - t


def timed_pair(path, off, on):
    """The fastest of 21 runs without and with the rules, interleaved so that drift hits both alike (for
    work that only uses the CPU, the minimum measures the work; medians move with the machine's load)."""
    a, b = [], []
    for _ in range(21):
        a.append(once(path, off))
        b.append(once(path, on))
    return min(a), min(b)


def main():
    on = dict(os.environ)
    off = dict(os.environ, VO_STRUCTURE_OFF="1")
    fails = 0
    with tempfile.TemporaryDirectory() as tmp:
        big = generated(tmp)
        lines = sum(1 for p in os.listdir(tmp) for _ in open(os.path.join(tmp, p)))
        targets = [("examples/helpdesk/main.vo", os.path.join(ROOT, "examples", "helpdesk", "main.vo")),
                   (f"generated ({lines} lines)", big)]
        for name, path in targets:
            a, b = timed_pair(path, off, on)
            ratio = b / a if a else 1.0
            ok = ratio <= 1.10
            fails += not ok
            print(f"{'PASS' if ok else 'FAIL'} {name}: {a * 1000:.1f} ms without the rules, {b * 1000:.1f} ms with "
                  f"({(ratio - 1) * 100:+.1f}%)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
