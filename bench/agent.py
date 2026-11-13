#!/usr/bin/env python3
"""Runs a headless agent (`claude -p`) on bench/tasks and records attempts per task.

For vo the agent's only context is `vo reference`; for Python, nothing. Each task gets up to
--attempts tries; after a failed try the agent sees its code plus the toolchain output (vo check
--json diagnostics, runtime errors) or the failing case. Results: bench/results/<lang>.jsonl.
"""
import argparse
import json
import os
import re
import subprocess
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
TASKS = os.path.join(ROOT, "bench", "tasks")


def ask(prompt, model):
    # An isolated agent that sees only the prompt: no tools (it could otherwise read the reference
    # solutions and hidden cases, or spend its time on refused tool calls), an empty working
    # directory, and none of the user's settings, hooks, plugins or MCP servers.
    cmd = ["claude", "-p", "--tools", "", "--setting-sources", "", "--strict-mcp-config",
           "--no-session-persistence"] + (["--model", model] if model else [])
    with tempfile.TemporaryDirectory() as empty:
        p = subprocess.run(cmd, input=prompt.encode(), capture_output=True, timeout=600, cwd=empty)
    return p.stdout.decode("utf-8", "replace")


def extract_code(reply):
    blocks = re.findall(r"```[\w-]*\n(.*?)```", reply, re.S)
    return blocks[-1] if blocks else reply


def cases(task_dir):
    d = os.path.join(task_dir, "cases")
    names = sorted(n[:-3] for n in os.listdir(d) if n.endswith(".in"))
    return [(open(os.path.join(d, n + ".in"), "rb").read(), open(os.path.join(d, n + ".out"), "rb").read()) for n in names]


def evaluate(lang, code, task_dir):
    """None when every case passes, else feedback text for the agent."""
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "solution.vo" if lang == "vo" else "solution.py")
        open(path, "w", encoding="utf-8").write(code)
        if lang == "vo":
            c = subprocess.run([VO, "check", "--json", path], capture_output=True, cwd=tmp, timeout=60)
            if c.returncode != 0:
                return "vo check --json reported:\n" + c.stdout.decode("utf-8", "replace")
            run = [VO, "run", "--timeout", "10", path]
        else:
            run = ["python3", path]
        for i, (inp, want) in enumerate(cases(task_dir), 1):
            try:
                r = subprocess.run(run, input=inp, capture_output=True, cwd=tmp, timeout=30)
            except subprocess.TimeoutExpired:
                return f"Case {i} timed out.\nInput:\n{inp.decode()}"
            if r.returncode != 0 or r.stdout != want:
                return (f"Case {i} failed (exit status {r.returncode}).\nInput:\n{inp.decode()}\n"
                        f"Expected output:\n{want.decode()}\nYour output:\n{r.stdout.decode('utf-8', 'replace')}\n"
                        f"Stderr:\n{r.stderr.decode('utf-8', 'replace')[:2000]}")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", choices=["vo", "python"], required=True)
    ap.add_argument("--model", default=None)
    ap.add_argument("--attempts", type=int, default=3)
    ap.add_argument("--only", default="", help="comma-separated task numbers, e.g. 01,02")
    a = ap.parse_args()
    only = {x.strip() for x in a.only.split(",") if x.strip()}
    reference = ""
    if a.lang == "vo":
        reference = subprocess.run([VO, "reference"], capture_output=True).stdout.decode()
    os.makedirs(os.path.join(ROOT, "bench", "results"), exist_ok=True)
    out = open(os.path.join(ROOT, "bench", "results", f"{a.lang}.jsonl"), "a")
    lang_name = "vo" if a.lang == "vo" else "Python 3"
    for name in sorted(os.listdir(TASKS)):
        task_dir = os.path.join(TASKS, name)
        if not os.path.isdir(task_dir) or (only and name[:2] not in only):
            continue
        task = open(os.path.join(task_dir, "task.md"), encoding="utf-8").read()
        base = ""
        if reference:
            base += "This is the complete reference of the vo programming language:\n\n" + reference + "\n\n"
        base += (f"Write a {lang_name} program for this task. It reads stdin and writes stdout.\n\n{task}\n\n"
                 "Reply with only the complete program in one fenced code block.")
        prompt, solved, attempts, start = base, False, 0, time.time()
        for attempts in range(1, a.attempts + 1):
            code = extract_code(ask(prompt, a.model))
            feedback = evaluate(a.lang, code, task_dir)
            if feedback is None:
                solved = True
                break
            prompt = (base + f"\n\nYour previous program:\n```\n{code}\n```\nIt failed:\n{feedback}\n"
                      "Fix it. Reply with only the complete corrected program in one fenced code block.")
        rec = {"task": name, "lang": a.lang, "model": a.model, "attempts": attempts, "solved": solved,
               "seconds": round(time.time() - start, 1)}
        out.write(json.dumps(rec) + "\n")
        out.flush()
        print(json.dumps(rec))


if __name__ == "__main__":
    main()
