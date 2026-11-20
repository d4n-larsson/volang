#!/usr/bin/env python3
"""Spec 029 SC-004: does std.web help agents build an API right the first time? Each run gives an
isolated agent (`claude -p`: no tools, an empty directory, no user settings) the vo reference and the
inventory task (task.md), takes the files it replies with, and grades them once with accept.py: a run
passes when every acceptance check passes. Two arms with the same reference and task: `web` (use
std.web) and `http` (use std.http's serve_workers directly, not std.web). Results:
bench/web/results/<arm>.jsonl, one record per run (passed, checks, seconds).

  run.py --arm web|http [--runs N] [--model M]
  run.py --summary
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

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
VO = os.path.join(ROOT, "bin", "vo")
sys.path.insert(0, os.path.join(ROOT, "bench2"))
from agent import parse_reply  # noqa: E402  (benchmark v2's reply format)

TIMEOUT = 1800  # a whole API in one reply takes about 15 minutes


def ask(text, model):
    """benchmark v2's isolated agent (no tools, an empty directory, no user settings), with a longer limit;
    "" when it does not answer in time (a failed run)."""
    cmd = ["claude", "-p", "--tools", "", "--setting-sources", "", "--strict-mcp-config",
           "--no-session-persistence"] + (["--model", model] if model else [])
    with tempfile.TemporaryDirectory() as empty:
        try:
            p = subprocess.run(cmd, input=text.encode(), capture_output=True, timeout=TIMEOUT, cwd=empty)
        except subprocess.TimeoutExpired:
            return ""
    return p.stdout.decode("utf-8", "replace")

ARMS = {
    "web": "Build it with std.web (the standard library's API package).",
    "http": "Build it with std.http's serve_workers directly; do not import std.web.",
}


def prompt(arm):
    reference = subprocess.run([VO, "reference"], capture_output=True, text=True).stdout
    statement = open(os.path.join(HERE, "task.md"), encoding="utf-8").read()
    return ("This is the complete reference of the vo programming language:\n\n" + reference + "\n\n"
            "Write a vo program for this task. The program's entry file is `main.vo`. " + ARMS[arm] + "\n\n"
            + statement + "\n\nReply with every file of the finished program, each in its own fenced code block "
            "whose first line is `# file: <path>` (for example `# file: main.vo`).")


def run_once(arm, model):
    start = time.time()
    reply = ask(prompt(arm), model)
    took = time.time() - start
    files = parse_reply(reply, "main.vo")
    with tempfile.TemporaryDirectory() as d:
        for p, text in files.items():
            os.makedirs(os.path.dirname(os.path.join(d, p)) or d, exist_ok=True)
            open(os.path.join(d, p), "w", encoding="utf-8").write(text)
        out = subprocess.run([sys.executable, os.path.join(HERE, "accept.py"), d], capture_output=True, text=True).stdout
        log = open(os.path.join(d, "server.log")).read() if os.path.exists(os.path.join(d, "server.log")) else ""
    m = re.search(r"accept: (\d+)/(\d+)", out)
    passed, total = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    uses_web = any("std.web" in t for t in files.values())
    return {"arm": arm, "passed": passed == total and total > 1, "checks": passed, "of": total,
            "seconds": round(took, 1), "files": len(files), "uses_std_web": uses_web,
            "failed": [l[5:] for l in out.splitlines() if l.startswith("FAIL ")], "log": log[-2000:], "reply": reply}


def summary():
    for arm in ARMS:
        path = os.path.join(HERE, "results", arm + ".jsonl")
        if not os.path.exists(path):
            continue
        rs = [json.loads(l) for l in open(path)]
        ok = sum(r["passed"] for r in rs)
        print(f"{arm}: {ok}/{len(rs)} passed first time ({ok / len(rs):.0%}), median checks "
              f"{sorted(r['checks'] for r in rs)[len(rs) // 2]}/{rs[0]['of'] or 29}, median "
              f"{sorted(r['seconds'] for r in rs)[len(rs) // 2]} s")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arm", choices=list(ARMS))
    ap.add_argument("--runs", type=int, default=20)
    ap.add_argument("--model")
    ap.add_argument("--summary", action="store_true")
    a = ap.parse_args()
    if a.summary:
        summary()
        return 0
    os.makedirs(os.path.join(HERE, "results"), exist_ok=True)
    with open(os.path.join(HERE, "results", a.arm + ".jsonl"), "a") as f:
        for i in range(a.runs):
            r = run_once(a.arm, a.model)
            f.write(json.dumps(r) + "\n")
            f.flush()
            print(f"{a.arm} run {i + 1}: {'PASS' if r['passed'] else 'FAIL'} {r['checks']}/{r['of']} in {r['seconds']} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
