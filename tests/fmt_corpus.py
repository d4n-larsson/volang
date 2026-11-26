#!/usr/bin/env python3
"""Formatter properties over the corpus (SC-005).

For every .vo file under tests/cases/ and bench/ that passes `vo check`:
- formatting twice gives byte-identical results (idempotence)
- run cases (tests/cases/run) behave the same before and after formatting: same stdout and exit
  status (stderr too when formatting changed nothing, since positions in reports may move)
Multi-file programs are formatted file by file inside a copy of their directory.
"""
import os
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")


def vo(args, cwd, stdin=b""):
    return subprocess.run([VO] + args, capture_output=True, cwd=cwd, input=stdin, timeout=120)


def corpus():
    for top in ("tests/cases", "bench"):
        for d, _, names in os.walk(os.path.join(ROOT, top)):
            for n in sorted(names):
                if n.endswith(".vo") and not n.endswith(".expected.vo"):
                    yield os.path.join(d, n)


def run_args(path):
    base = path[:-3]
    if os.path.exists(base + ".cmd"):
        cmd = open(base + ".cmd").read().split()
        if not cmd or cmd[0] != "run":
            return None
        return [a if a != "{}" else os.path.basename(path) for a in cmd]
    return ["run", os.path.basename(path)]


def one(path):
    """(checked, problems) for one corpus file."""
    d = os.path.dirname(path)
    if vo(["check", os.path.basename(path)], d).returncode != 0:
        return 0, []
    problems = []
    runs = os.sep + os.path.join("tests", "cases", "run") + os.sep in path  # behavior compared: the whole folder
    with tempfile.TemporaryDirectory() as tmp:
        orig_dir = os.path.join(tmp, "orig")
        fmt_dir = os.path.join(tmp, "fmt")
        name = os.path.basename(path)
        if runs:
            shutil.copytree(d, orig_dir)
            shutil.copytree(d, fmt_dir)
        else:  # vo fmt reads only the file
            os.makedirs(fmt_dir)
            shutil.copy(path, fmt_dir)
        r = vo(["fmt", name], fmt_dir)
        if r.returncode != 0:
            return 1, [f"{path}: vo fmt exited {r.returncode}: {r.stdout.decode()[:200]}"]
        once = open(os.path.join(fmt_dir, name), "rb").read()
        vo(["fmt", name], fmt_dir)
        twice = open(os.path.join(fmt_dir, name), "rb").read()
        if once != twice:
            problems.append(f"{path}: formatting is not idempotent")
        if not runs:
            return 1, problems
        args = run_args(path)
        base = path[:-3]
        if args is None or os.path.exists(base + ".hold-stdin"):
            return 1, problems
        stdin = open(base + ".in", "rb").read() if os.path.exists(base + ".in") else b""
        a = vo(args, orig_dir, stdin)
        b = vo(args, fmt_dir, stdin)
        changed = once != open(path, "rb").read()
        if (a.returncode, a.stdout) != (b.returncode, b.stdout) or (not changed and a.stderr != b.stderr):
            problems.append(f"{path}: behavior changed after formatting")
    return 1, problems


def main():
    jobs = int(os.environ.get("VO_JOBS") or os.cpu_count() or 1)  # files are independent; results stay in order
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(one, corpus()))
    checked = sum(c for c, _ in results)
    problems = [p for _, ps in results for p in ps]
    for p in problems:
        print("FAIL", p)
    print(f"fmt_corpus: {checked} files, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
