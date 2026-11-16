#!/usr/bin/env python3
"""Spec 014 US6 (FR-015, FR-016, SC-004, SC-005): tasks that need an ordered map or set, a priority
queue or a deque.

  structures_eval.py                         every task: with.vo and without.vo pass every test (the
                                             expected output is with.vo's, and without.vo must give the
                                             same), naive.vo passes the visible tests and fails (times
                                             out) on a large hidden one
  structures_eval.py --lines                 SC-004: lines of with.vo against without.vo, per task and median
  structures_eval.py --solve [--without-module] [--only s01,s02] [--model M] [--rounds 3]
                                             isolated agents (tests/stdlib_eval.py's protocol: `claude -p`,
                                             no tools, empty working directory, no user settings), with the
                                             std.collections documentation in the prompt or without it;
                                             results appended to tests/structures_eval/solve.jsonl
  structures_eval.py --score                 the agents' saved solutions (main.vo, main_without.vo) on
                                             every test

Each task (tests/structures_eval/sNN-*/) has task.md (the statement), gen.py (`tests()`: a list of
{"name", "stdin", "hidden"}; deterministic), and three reference solutions never shown to an agent:
with.vo (std.collections), without.vo (list/map only, with the structure written by hand) and
naive.vo (sorts or scans on every step).
"""
import importlib.util
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
TASKS = os.path.join(ROOT, "tests", "structures_eval")
TIMEOUT = "10"
sys.path.insert(0, os.path.join(ROOT, "bench2"))


def tasks(only):
    for name in sorted(os.listdir(TASKS)):
        d = os.path.join(TASKS, name)
        if os.path.isdir(d) and name[:1] == "s" and (not only or name.split("-")[0] in only or name in only):
            yield name, d


def gen(d):
    spec = importlib.util.spec_from_file_location("gen", os.path.join(d, "gen.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m.tests()


def run(program, stdin):
    """(stdout, exit status) of the program on stdin, with the time limit."""
    with tempfile.TemporaryDirectory() as work:
        open(os.path.join(work, "main.vo"), "w", encoding="utf-8").write(program)
        r = subprocess.run([VO, "run", "--timeout", TIMEOUT, "main.vo"], cwd=work, input=stdin.encode(),
                           capture_output=True, timeout=120)
        return r.stdout.decode("utf-8", "replace"), r.returncode


def source(d, name):
    return open(os.path.join(d, name), encoding="utf-8").read()


def lines(text):
    return sum(1 for l in text.splitlines() if l.strip() and not l.strip().startswith("#"))


def check_references(only):
    bad = 0
    for name, d in tasks(only):
        ts = gen(d)
        problems, naive_failed = [], False
        for t in ts:
            want, code = run(source(d, "with.vo"), t["stdin"])
            if code:
                problems.append(f"{t['name']}: with.vo exits {code}")
            got, code2 = run(source(d, "without.vo"), t["stdin"])
            if (got, code2) != (want, code):
                problems.append(f"{t['name']}: without.vo differs (exit {code2})")
            ngot, ncode = run(source(d, "naive.vo"), t["stdin"])
            if (ngot, ncode) != (want, code):
                if t["hidden"]:
                    naive_failed = True
                else:
                    problems.append(f"{t['name']}: naive.vo fails a visible test (exit {ncode})")
        if not naive_failed:
            problems.append("naive.vo passes every hidden test (the large inputs do not separate it)")
        bad += bool(problems)
        for p in problems:
            print(f"FAIL {name}: {p}")
        print(f"{name:28} {len(ts)} tests" + ("" if problems else ", references ok"))
    print(f"structures_eval: {bad} task(s) with problems")
    return 1 if bad else 0


def line_counts(only):
    ratios = []
    for name, d in tasks(only):
        w, wo = lines(source(d, "with.vo")), lines(source(d, "without.vo"))
        ratios.append(w / wo)
        print(f"{name:28} with {w:4} lines, without {wo:4} lines, {100 - 100 * w / wo:5.1f}% shorter")
    m = statistics.median(ratios)
    print(f"median: with.vo is {100 - 100 * m:.1f}% shorter (SC-004: at least 30%)")
    return 0 if m <= 0.7 else 1


def doc_text(without):
    if without:
        return ""
    out = subprocess.run([VO, "doc", "std.collections"], capture_output=True).stdout.decode()
    return ("This is the documentation of vo's std.collections module, printed by `vo doc std.collections`:\n\n"
            + out + "\n\n")


def failures(d, program, ts):
    out = []
    for t in ts:
        want, code = run(source(d, "with.vo"), t["stdin"])
        got, c = run(program, t["stdin"])
        if (got, c) != (want, code):
            out.append((t, got, c, want, code))
    return out


def solve(only, model, rounds, without):
    import agent  # bench2/agent.py: ask() and parse_reply(), the isolated protocol
    reference = subprocess.run([VO, "reference"], capture_output=True).stdout.decode()
    log = open(os.path.join(TASKS, "solve.jsonl"), "a")
    for name, d in tasks(only):
        ts = gen(d)
        visible = [t for t in ts if not t["hidden"]]
        statement = source(d, "task.md")
        shown = "\n".join(f"\nTest {i}: stdin:\n```\n{t['stdin']}```\nexpected stdout:\n```\n{run(source(d, 'with.vo'), t['stdin'])[0]}```"
                          for i, t in enumerate(visible, 1))
        base = ("This is the complete reference of the vo programming language:\n\n" + reference + "\n\n"
                + doc_text(without) + "Write a vo program for this task. The program's entry file is `main.vo`. "
                f"It runs with `vo run --timeout {TIMEOUT} main.vo` on hidden tests too, some with large inputs.\n\n"
                + statement + "\n\nVisible tests:" + shown + "\n\nReply with the program in one fenced code block "
                "whose first line is `# file: main.vo`.")
        prompt, start, passed, program = base, time.time(), False, ""
        for attempt in range(1, rounds + 1):
            program = agent.parse_reply(agent.ask(prompt, model), "main.vo").get("main.vo", "")
            fails = [f for f in failures(d, program, visible)]
            if not fails:
                passed = True
                break
            text = "\n\n".join(f"stdin:\n```\n{t['stdin'][:2000]}```\nyour stdout (exit {c}):\n```\n{got[:2000]}```\n"
                               f"expected (exit {code}):\n```\n{want[:2000]}```" for t, got, c, want, code in fails)
            prompt = (base + "\n\nYour previous program:\n\n```\n# file: main.vo\n" + program + "```\n\nIt did not "
                      "pass:\n\n" + text + "\n\nFix it. Reply with the corrected program, as before.")
        hidden_fails = len(failures(d, program, [t for t in ts if t["hidden"]])) if passed else None
        target = "main_without.vo" if without else "main.vo"
        open(os.path.join(d, target), "w", encoding="utf-8").write(program)
        rec = {"task": name, "module": not without, "model": model, "rounds": attempt, "visible_passed": passed,
               "hidden_failed": hidden_fails, "lines": lines(program), "seconds": round(time.time() - start, 1)}
        log.write(json.dumps(rec) + "\n")
        log.flush()
        print(json.dumps(rec), flush=True)


def score(only):
    for name, d in tasks(only):
        ts = gen(d)
        for target in ("main.vo", "main_without.vo"):
            p = os.path.join(d, target)
            if os.path.exists(p):
                n = len(failures(d, source(d, target), ts))
                print(f"{name:28} {target:16} {len(ts) - n}/{len(ts)} tests")


def main():
    a = sys.argv[1:]
    only = set(a[a.index("--only") + 1].split(",")) if "--only" in a else set()
    if "--lines" in a:
        return line_counts(only)
    if "--solve" in a:
        model = a[a.index("--model") + 1] if "--model" in a else None
        rounds = int(a[a.index("--rounds") + 1]) if "--rounds" in a else 3
        solve(only, model, rounds, "--without-module" in a)
        return 0
    if "--score" in a:
        score(only)
        return 0
    return check_references(only)


if __name__ == "__main__":
    sys.exit(main())
