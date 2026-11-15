#!/usr/bin/env python3
"""Spec 007 US6 (SC-001): task programs written only from their statements and `vo doc` output.

  stdlib_eval.py                       runs every task's saved solution (main.vo) on its tests
  stdlib_eval.py --record              writes the expected outputs from the maintainer's reference.vo
  stdlib_eval.py --solve [--only t01,t02] [--model M] [--rounds 3]
                                       asks an isolated agent for each solution (bench2's protocol:
                                       `claude -p` with no tools, an empty working directory and no
                                       user settings), feeding check and test failures back

Each task (tests/stdlib_eval/tNN-*/) has task.md (the statement), tests.json (visible tests:
flags, args, stdin, and after --record the expected stdout and exit status), fixtures/ (copied
next to the program), and reference.vo (the maintainer's solution, used only for --record; never
shown to the agent). A task with "package": true pins its fixtures' shared/money as package `money`;
one with "server": true runs against tests/http_server.py ("{port}" in args is its port).
The agent sees the statement, the visible tests, `vo reference` and `vo doc` output: the module
list, every item of every standard module with its example, and the task's package modules.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
TASKS = os.path.join(ROOT, "tests", "stdlib_eval")
sys.path.insert(0, os.path.join(ROOT, "bench2"))


def tasks(only):
    for name in sorted(os.listdir(TASKS)):
        d = os.path.join(TASKS, name)
        if os.path.isdir(d) and re.match(r"t\d\d-", name) and (not only or name.split("-")[0] in only or name in only):
            yield name, d


def prepare(task_dir, meta, program, work):
    """The program's directory: fixtures, main.vo, and the pinned package if the task has one."""
    fx = os.path.join(task_dir, "fixtures")
    if os.path.isdir(fx):
        shutil.copytree(fx, work, dirs_exist_ok=True)
    open(os.path.join(work, "main.vo"), "w", encoding="utf-8").write(program)
    if meta.get("package"):
        subprocess.run([VO, "package", "main.vo", "money", "shared/money"], cwd=work, capture_output=True, check=True)


class Server:
    def __enter__(self):
        self.p = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "http_server.py"), "0"],
                                  stdout=subprocess.PIPE)
        self.port = int(self.p.stdout.readline().split()[1])
        return self

    def __exit__(self, *a):
        self.p.kill()
        self.p.wait()


def run_tests(task_dir, meta, program):
    """[(test, stdout, exit status)] for the program on every test."""
    out = []
    with tempfile.TemporaryDirectory() as work:
        prepare(task_dir, meta, program, work)
        server = Server() if meta.get("server") else None
        port = server.__enter__().port if server else None
        try:
            for t in meta["tests"]:
                args = [a.replace("{port}", str(port)) for a in t["args"]]
                r = subprocess.run([VO, "run"] + t["flags"] + ["main.vo", "--"] + args, cwd=work,
                                   input=t["stdin"].encode(), capture_output=True, timeout=60)
                out.append((t, r.stdout.decode("utf-8", "replace"), r.returncode))
        finally:
            if server:
                server.__exit__()
    return out


def command(t):
    return " ".join(["vo", "run"] + t["flags"] + ["main.vo"] + (["--"] + t["args"] if t["args"] else []))


def visible(meta):
    parts = ["Visible tests (stdout must match exactly, and the exit status):"]
    for i, t in enumerate(meta["tests"], 1):
        parts.append(f"\nTest {i}: `{command(t)}`" + (" (`{port}` is the service's port)" if meta.get("server") else ""))
        parts.append("stdin:\n```\n" + t["stdin"] + "```" if t["stdin"] else "stdin: (empty)")
        parts.append("expected stdout:\n```\n" + t["expected"] + "```" if t["expected"] else "expected stdout: (empty)")
        parts.append(f"expected exit status: {t['code']}")
    return "\n".join(parts)


def doc_text(task_dir, meta):
    """`vo doc` output: the module list, every standard item in full, and the task's package."""
    def doc(args, cwd=ROOT):
        return subprocess.run([VO, "doc"] + args, capture_output=True, cwd=cwd).stdout.decode()
    parts = ["$ vo doc\n" + doc([])]
    items = [json.loads(l) for l in doc(["--json"]).splitlines() if l.strip()]
    modules = [r["name"] for r in items if r.get("item") == "module" and r["name"].startswith("std.")]
    for m in modules:
        recs = [json.loads(l) for l in doc(["--json", m]).splitlines() if l.strip()]
        for r in recs:
            if r.get("kind") == "doc":
                name = m + "." + r["name"]
                parts.append(f"$ vo doc {name}\n" + doc([name]))
    if meta.get("package"):
        with tempfile.TemporaryDirectory() as work:
            prepare(task_dir, meta, "", work)
            parts.append("$ vo doc money.format\n" + doc(["money.format"], work))
    return "\n".join(parts)


def failures(results):
    out = []
    for i, (t, stdout, code) in enumerate(results, 1):
        if stdout != t["expected"] or code != t["code"]:
            out.append(f"Test {i} (`{command(t)}`): exit status {code} (expected {t['code']}); stdout:\n```\n{stdout}```\n"
                       f"expected stdout:\n```\n{t['expected']}```")
    return out


def record(only):
    for name, d in tasks(only):
        path = os.path.join(d, "tests.json")
        meta = json.load(open(path))
        results = run_tests(d, meta, open(os.path.join(d, "reference.vo"), encoding="utf-8").read())
        for t, stdout, code in results:
            t["expected"], t["code"] = stdout, code
        open(path, "w").write(json.dumps(meta, indent=1, ensure_ascii=False) + "\n")
        print(f"recorded {name}: {len(results)} tests")


def solve(only, model, rounds):
    import agent  # bench2/agent.py: ask() and parse_reply(), the isolated protocol
    reference = subprocess.run([VO, "reference"], capture_output=True).stdout.decode()
    log = open(os.path.join(TASKS, "solve.jsonl"), "a")
    for name, d in tasks(only):
        meta = json.load(open(os.path.join(d, "tests.json")))
        statement = open(os.path.join(d, "task.md"), encoding="utf-8").read()
        base = ("This is the complete reference of the vo programming language:\n\n" + reference + "\n\n"
                "This is the documentation of vo's standard library (and of the task's package, if any), "
                "printed by `vo doc`:\n\n" + doc_text(d, meta) + "\n\n"
                "Write a vo program for this task. The program's entry file is `main.vo`.\n\n" + statement + "\n\n"
                + visible(meta) + "\n\nReply with the program in one fenced code block whose first line is "
                "`# file: main.vo`.")
        prompt, start, passed, files = base, time.time(), False, {}
        for attempt in range(1, rounds + 1):
            files = agent.parse_reply(agent.ask(prompt, model), "main.vo")
            program = files.get("main.vo", "")
            with tempfile.TemporaryDirectory() as work:
                prepare(d, meta, program, work)
                check = subprocess.run([VO, "check", "main.vo"], cwd=work, capture_output=True).stdout.decode()
            fails = failures(run_tests(d, meta, program))
            if not fails:
                passed = True
                break
            prompt = (base + "\n\nYour previous program:\n\n```\n# file: main.vo\n" + program + "```\n\n"
                      "`vo check main.vo` printed:\n```\n" + check + "```\n\nIt did not pass:\n\n" + "\n\n".join(fails)
                      + "\n\nFix it. Reply with the corrected program, as before.")
        open(os.path.join(d, "main.vo"), "w", encoding="utf-8").write(files.get("main.vo", ""))
        rec = {"task": name, "model": model, "rounds": attempt, "passed": passed, "seconds": round(time.time() - start, 1)}
        log.write(json.dumps(rec) + "\n")
        log.flush()
        print(json.dumps(rec), flush=True)


def evaluate(only):
    bad, rows = 0, []
    for name, d in tasks(only):
        meta = json.load(open(os.path.join(d, "tests.json")))
        sol = os.path.join(d, "main.vo")
        if not os.path.exists(sol):
            rows.append((name, "no solution"))
            bad += 1
            continue
        fails = failures(run_tests(d, meta, open(sol, encoding="utf-8").read()))
        rows.append((name, f"{len(meta['tests']) - len(fails)}/{len(meta['tests'])} tests"))
        bad += bool(fails)
        for f in fails:
            print(f"FAIL {name}: {f}")
    for name, what in rows:
        print(f"{name:22} {what}")
    print(f"stdlib_eval: {len(rows) - bad} of {len(rows)} tasks pass all their tests")
    return 1 if bad else 0


def main():
    a = sys.argv[1:]
    only = set(a[a.index("--only") + 1].split(",")) if "--only" in a else set()
    if "--record" in a:
        record(only)
        return 0
    if "--solve" in a:
        model = a[a.index("--model") + 1] if "--model" in a else None
        rounds = int(a[a.index("--rounds") + 1]) if "--rounds" in a else 3
        solve(only, model, rounds)
        return 0
    return evaluate(only)


if __name__ == "__main__":
    sys.exit(main())
