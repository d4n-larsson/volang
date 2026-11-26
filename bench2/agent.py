#!/usr/bin/env python3
"""Runs a headless agent (`claude -p`) on benchmark v2 (contracts/benchmark-v2.md).

The prompt holds the task statement, the starter tree (refactor tasks) and the visible cases; for
vo also `vo reference`. The agent replies with fenced blocks that start with `# file: <path>` (one
block may omit it: it is the entry file). After a failed attempt the agent sees the toolchain
output, the failing visible cases, how many hidden checks failed (never which), and any capability
violation. Results: bench2/results/<lang>[-features].jsonl, one record per task; each attempt's files
in bench2/results/drafts/<lang>[-features]/<task>/<attempt>/.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")


def ask(prompt, model):
    # An isolated agent that sees only the prompt: no tools (it could otherwise read the reference
    # solutions and hidden cases, or spend its time on refused tool calls), an empty working
    # directory, and none of the user's settings, hooks, plugins or MCP servers.
    cmd = ["claude", "-p", "--tools", "", "--setting-sources", "", "--strict-mcp-config",
           "--no-session-persistence"] + (["--model", model] if model else [])
    # The API sometimes stalls: a call that hangs is asked again, up to three times, without using up
    # an attempt. Returns (reply, hangs); the reply is None when all three hung.
    for hangs in range(3):
        with tempfile.TemporaryDirectory() as empty:
            try:
                p = subprocess.run(cmd, input=prompt.encode(), capture_output=True, timeout=300, cwd=empty)
                return p.stdout.decode("utf-8", "replace"), hangs
            except subprocess.TimeoutExpired:
                pass
    return None, 3


def parse_reply(reply, entry):
    """{path: text} from the reply's fenced blocks."""
    files = {}
    for body in re.findall(r"```[\w.+-]*\n(.*?)```", reply, re.S):
        m = re.match(r"\s*(?:#|//)\s*file:\s*(\S+)\s*\n", body)
        if m:
            files[m.group(1)] = body[m.end():]
        elif entry not in files:
            files[entry] = body
    return {p: t for p, t in files.items() if not os.path.isabs(p) and ".." not in p.split("/")}


def tree_text(tree):
    out = []
    for dirpath, _, names in sorted(os.walk(tree)):
        for n in sorted(names):
            p = os.path.join(dirpath, n)
            rel = os.path.relpath(p, tree)
            out.append(f"```\n# file: {rel}\n{open(p, encoding='utf-8').read()}```")
    return "\n\n".join(out)


def visible_text(task_dir):
    parts = []
    for name, inp, want in grade.cases(task_dir, "visible"):
        parts.append(f"Example {name} input:\n```\n{inp.decode()}```\nExpected output:\n```\n{want.decode()}```")
    return "\n\n".join(parts)


def feedback(r):
    lines = []
    if r["toolchain_output"]:
        lines.append("The toolchain reported:\n" + r["toolchain_output"][:6000])
    for f in r["visible_failures"][:3]:
        if f["case"] == "check":
            continue
        lines.append(f"Example {f['case']} failed (exit status {f['exit']}).\nInput:\n{f['input']}\n"
                     f"Expected output:\n{f['expected']}\nYour output:\n{f['actual']}\nStderr:\n{f['stderr'][:2000]}")
    failed = r["hidden_total"] - r["hidden_passed"]
    if failed and not r["toolchain_output"]:
        lines.append(f"{failed} of {r['hidden_total']} hidden checks failed.")
    if r["declared_violation"]:
        lines.append("Capability violation: the program " + "; ".join(r["declared_violation"]) + ".")
    if r["behavioral_violation"]:
        lines.append("Capability violation: the program " + "; ".join(r["behavioral_violation"]) + ".")
    return "\n\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", choices=["vo", "python"], required=True)
    ap.add_argument("--set", choices=["tasks", "features"], default="tasks")
    ap.add_argument("--model", default=None)
    ap.add_argument("--attempts", type=int, default=3)
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    only = {x for x in a.only.split(",") if x}
    base = os.path.join(ROOT, "bench2", a.set)
    reference = ""
    if a.lang == "vo":
        reference = subprocess.run([VO, "reference"], capture_output=True).stdout.decode()
    os.makedirs(os.path.join(ROOT, "bench2", "results"), exist_ok=True)
    suffix = "-features" if a.set == "features" else ""
    out = open(os.path.join(ROOT, "bench2", "results", f"{a.lang}{suffix}.jsonl"), "a")
    entry = "main.vo" if a.lang == "vo" else "main.py"
    lang_name = "vo" if a.lang == "vo" else "Python 3"
    for name in sorted(os.listdir(base)):
        task_dir = os.path.join(base, name)
        if not os.path.isdir(task_dir) or (only and name not in only):
            continue
        category = open(os.path.join(task_dir, "category")).read().strip()
        statement = open(os.path.join(task_dir, "task.md"), encoding="utf-8").read()
        prompt = ""
        if reference:
            prompt += "This is the complete reference of the vo programming language:\n\n" + reference + "\n\n"
        prompt += (f"Write a {lang_name} program for this task. The program's entry file is `{entry}`; "
                   "it reads stdin and writes stdout.\n\n" + statement + "\n\n" + visible_text(task_dir))
        starter = os.path.join(task_dir, "starter", a.lang)
        if os.path.isdir(starter):
            prompt += "\n\nThe existing code:\n\n" + tree_text(starter)
        prompt += ("\n\nReply with every file of the finished program, each in its own fenced code block whose "
                   f"first line is `# file: <path>` (for example `# file: {entry}`). Files you do not send are "
                   "removed.")
        base_prompt, solved, attempts, start = prompt, False, 0, time.time()
        r, codes, timeouts = None, [], 0
        drafts = os.path.join(ROOT, "bench2", "results", "drafts", a.lang + suffix, name)
        shutil.rmtree(drafts, ignore_errors=True)
        for attempts in range(1, a.attempts + 1):
            reply, hangs = ask(prompt, a.model)
            timeouts += hangs
            files = parse_reply(reply or "", entry)
            # Every draft is kept, graded where it lies: results/drafts/<lang>[-features]/<task>/<attempt>/.
            sol = os.path.join(drafts, str(attempts))
            os.makedirs(sol)
            for p, text in files.items():
                os.makedirs(os.path.dirname(os.path.join(sol, p)) or sol, exist_ok=True)
                open(os.path.join(sol, p), "w", encoding="utf-8").write(text)
            r = grade.grade(task_dir, sol, a.lang)
            codes.append(r["codes"])
            if grade.solved(r):
                solved = True
                break
            sent = "\n\n".join(f"```\n# file: {p}\n{t}```" for p, t in sorted(files.items()))
            prompt = (base_prompt + "\n\nYour previous program:\n\n" + sent + "\n\nIt did not pass:\n\n" + feedback(r)
                      + "\n\nFix it. Reply with every file of the corrected program, as before.")
        rec = {"task": name, "category": category, "lang": a.lang, "model": a.model, "attempts": attempts,
               "solved": solved, "hidden_passed": r["hidden_passed"], "hidden_total": r["hidden_total"],
               "declared_violation": bool(r["declared_violation"]),
               "behavioral_violation": bool(r["behavioral_violation"]), "codes": codes,
               "timeouts": timeouts,
               "seconds": round(time.time() - start, 1)}
        out.write(json.dumps(rec) + "\n")
        out.flush()
        print(json.dumps(rec), flush=True)


if __name__ == "__main__":
    main()
