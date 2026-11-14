#!/usr/bin/env python3
"""Spec 005 SC-001/SC-002 stand-in: do property tests catch mistakes the visible cases miss?

For each tests/prop_eval/hNN-*/ (the bench2 hidden-property task of the same name):
  1. props.vo must pass on the reference solution, three times byte-identically.
  2. Each bugs/*.vo is a mutant of one reference file (first line `# replaces: <file>`). It must
     pass every visible case and fail at least one hidden case, or it is not a valid mutant.
     It is caught when `vo test props.vo` exits 1 on it.
Exit 1 when the properties catch less than 70% of the mutants overall, or none of a task's.

A task whose reference keeps its logic in `main` (which a property cannot call) has its own copy
in tests/prop_eval/hNN-*/ref/: the same program with that logic moved into functions. It must
pass every visible and hidden case; mutants are then made of it. bench2/ itself is never changed.

  prop_eval.py [TASK...]   only the named tasks (e.g. h01-interval-merge)
"""
import concurrent.futures as cf
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
EVAL = os.path.join(ROOT, "tests", "prop_eval")
TASKS = os.path.join(ROOT, "bench2", "tasks")


def program(task, props, bug=None):
    """A temporary directory with the reference solution, props.vo and the mutant applied."""
    d = tempfile.mkdtemp()
    shutil.copytree(os.path.join(TASKS, task, "ref", "vo"), d, dirs_exist_ok=True)
    own = os.path.join(EVAL, task, "ref")
    if os.path.isdir(own):
        shutil.copytree(own, d, dirs_exist_ok=True)
    shutil.copy(props, os.path.join(d, "props.vo"))
    if bug:
        text = open(bug, encoding="utf-8").read()
        first = text.split("\n", 1)[0]
        assert first.startswith("# replaces: "), f"{bug}: first line must be `# replaces: <file>`"
        open(os.path.join(d, first[len("# replaces: "):].strip()), "w", encoding="utf-8").write(text)
    return d


def run_case(d, inp):
    r = subprocess.run([VO, "run", "--timeout", "30", "main.vo"], cwd=d, input=open(inp, "rb").read(),
                       capture_output=True, timeout=120)
    return r.stdout


def cases_pass(task, d, kind):
    """(passed, total) over the task's visible or hidden cases."""
    cdir = os.path.join(TASKS, task, kind)
    names = sorted(n[:-3] for n in os.listdir(cdir) if n.endswith(".in"))
    ok = sum(run_case(d, os.path.join(cdir, n + ".in")) == open(os.path.join(cdir, n + ".out"), "rb").read()
             for n in names)
    return ok, len(names)


def vo_test(d):
    r = subprocess.run([VO, "test", "--timeout", "60", "props.vo"], cwd=d, capture_output=True, timeout=300)
    return r.returncode, re.sub(rb"\d+ms\)", b"0ms)", r.stdout)  # timings differ between runs


def judge(task, props, bug):
    d = program(task, props, bug)
    try:
        vis, nvis = cases_pass(task, d, "visible")
        hid, nhid = cases_pass(task, d, "hidden")
        code, out = vo_test(d)
    finally:
        shutil.rmtree(d)
    problem = None
    if vis != nvis:
        problem = f"fails {nvis - vis} visible case(s)"
    elif hid == nhid:
        problem = "passes every hidden case"
    return os.path.basename(bug), problem, code == 1, out


def main():
    problems = []
    rows = []
    total = caught = 0
    for name in sorted(os.listdir(EVAL)):
        tdir = os.path.join(EVAL, name)
        props = os.path.join(tdir, "props.vo")
        if not os.path.isfile(props) or (sys.argv[1:] and name not in sys.argv[1:]):
            continue
        d = program(name, props)
        try:
            runs = [vo_test(d) for _ in range(3)]
            if os.path.isdir(os.path.join(tdir, "ref")):
                for kind in ("visible", "hidden"):
                    ok, n = cases_pass(name, d, kind)
                    if ok != n:
                        problems.append(f"{name}: the refactored ref/ fails {n - ok} {kind} case(s)")
        finally:
            shutil.rmtree(d)
        if any(code != 0 for code, _ in runs):
            problems.append(f"{name}: props.vo fails on the reference solution:\n{runs[0][1].decode()}")
        if len({out for _, out in runs}) != 1:
            problems.append(f"{name}: three runs on the reference differ")
        bdir = os.path.join(tdir, "bugs")
        bugs = sorted(os.path.join(bdir, b) for b in os.listdir(bdir) if b.endswith(".vo"))
        with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            results = list(ex.map(lambda b: judge(name, props, b), bugs))
        n_caught = 0
        for bug, problem, hit, out in results:
            if problem:
                problems.append(f"{name}/{bug}: not a valid mutant: {problem}")
            n_caught += hit
        if len(bugs) < 5:
            problems.append(f"{name}: {len(bugs)} mutants, at least 5 needed")
        rows.append((name, len(bugs), n_caught, [b for b, _, hit, _ in results if not hit]))
        total += len(bugs)
        caught += n_caught
    print(f"{'task':28} {'mutants':>7} {'visible':>7} {'props':>5}  missed")
    for name, n, c, missed in rows:
        print(f"{name:28} {n:7} {0:7} {c:5}  {' '.join(missed)}")
        if c == 0:
            problems.append(f"{name}: the properties catch none of its mutants")
    pct = 100 * caught / total if total else 0
    print(f"total: {caught} of {total} mutants caught by properties ({pct:.0f}%), 0 by the visible cases")
    if total and caught * 10 < total * 7:
        problems.append(f"properties catch {pct:.0f}% < 70%")
    for p in problems:
        print("FAIL", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
