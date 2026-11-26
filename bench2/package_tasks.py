#!/usr/bin/env python3
"""Spec 061 SC-008: can an agent that sees only `vo reference` and command output manage packages?

Three tasks against tests/git_server.py (the repositories of tests/cases/packages/graph): add a package
with a dependency, update a package, restore a clone. Unlike the other bench2 tasks the agent writes no
code: each turn it replies with one command line starting with `vo ` in a fenced block (run here without
a shell, in the task's project directory), or with DONE and its answer. It then sees the command's
output. The agent is isolated as in agent.py (no tools, empty directory, no user settings).

  package_tasks.py [--model M] [--runs N] [--only add,update,restore]

Per run: solved or not, the commands, and how many failed with a usage error (status 64): those are the
failures the command's interface caused. Results: bench2/results/package-tasks.jsonl. Not part of make test.
"""
import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import agent  # noqa: E402
import git_server  # noqa: E402

VO = os.path.join(ROOT, "bin", "vo")
GRAPH = os.path.join(ROOT, "tests", "cases", "packages", "graph")
TURNS = 8
RULES = """You work in a project directory through one command per reply. Reply with exactly one fenced block holding
one command line that starts with `vo ` (no shell syntax: no pipes, no `&&`, no redirection). You will then get its
output and exit status. The package host is a local test server: every `vo package` command that fetches needs
`--ca-file test-ca.pem`. When the task is done, reply with the word DONE on its own line, then your answer if the
task asks for one. You have at most 8 commands."""


def vo(d, *words):
    return subprocess.run([VO, *words], cwd=d, capture_output=True, text=True, timeout=120)


def setup_add(d, base):
    shutil.copy(os.path.join(GRAPH, "main.vo"), d)
    return (f"main.vo imports `report.table`, a package that is not installed. Its repository is {base}/report. "
            "Add it so that `vo run main.vo` works, then run the program.")


def check_add(d, base, answer, before):
    text = open(os.path.join(d, "vo.packages")).read() if os.path.exists(os.path.join(d, "vo.packages")) else ""
    return vo(d, "run", "main.vo").stdout == "total 12,34\n" and all(n in text for n in ("report ", "money ", "pads "))


def setup_update(d, base):
    open(os.path.join(d, "main.vo"), "w").write(
        'import money.format;\n\nfn main() uses console {\n    print(format.cents(1234));\n}\n')
    vo(d, "package", "--ca-file", "test-ca.pem", "main.vo", "money", f"{base}/money@1.2.0")
    return ("This program uses the package `money` at version 1.2.0. Find out whether newer versions exist, move "
            "`money` to the newest version that is compatible with 1.2.0 (not to a new major version), and answer with "
            "the version you moved to and the effects the package gained, if any.")


def check_update(d, base, answer, before):
    text = open(os.path.join(d, "vo.packages")).read()
    return "/money@1.6.0" in text and "1.6.0" in answer and "clock" in answer


def setup_restore(d, base):
    shutil.copy(os.path.join(GRAPH, "main.vo"), d)
    vo(d, "package", "--ca-file", "test-ca.pem", "main.vo", "report", f"{base}/report")
    shutil.rmtree(os.path.join(d, "packages"))
    return ("This project was just cloned: `vo.packages` is there, the package directories are not. Make "
            "`vo run main.vo` work without changing `vo.packages`, then run the program.")


def check_restore(d, base, answer, before):
    return vo(d, "run", "main.vo").stdout == "total 12,34\n" and open(os.path.join(d, "vo.packages")).read() == before


TASKS = {"add": (setup_add, check_add), "update": (setup_update, check_update), "restore": (setup_restore, check_restore)}


def run_task(name, base, model, reference):
    setup, check = TASKS[name]
    d = tempfile.mkdtemp(prefix=f"vo-pkgtask-{name}-")
    try:
        shutil.copy(os.path.join(GRAPH, "test-ca.pem"), d)
        statement = setup(d, base)
        manifest = os.path.join(d, "vo.packages")
        before = open(manifest).read() if os.path.exists(manifest) else None
        files = sorted(n for n in os.listdir(d))
        log = (f"{RULES}\n\n# Task\n\n{statement}\n\nFiles in the project directory: {', '.join(files)}\n\n"
               f"# The language and toolchain reference (`vo reference`)\n\n{reference}\n")
        commands, usage_errors, answer = [], 0, ""
        for _ in range(TURNS + 1):
            reply, _ = agent.ask(log + "\n# Your next reply\n", model)
            if reply is None:
                break
            if re.search(r"^\s*DONE\b", reply, re.M):
                answer = reply
                break
            m = re.search(r"```[\w.+-]*\n\s*(vo [^\n]*)\n", reply)
            if not m or len(commands) == TURNS:
                log += "\n# Harness\n\nNo command found (or none left). Reply with one `vo ...` line in a fenced block, or DONE.\n"
                if len(commands) == TURNS:
                    break
                continue
            line = m.group(1).strip()
            try:
                words = shlex.split(line)[1:]
                r = vo(d, *words)
                out, status = r.stdout + r.stderr, r.returncode
            except (ValueError, subprocess.TimeoutExpired) as e:
                out, status = str(e), -1
            commands.append({"command": line, "status": status})
            usage_errors += status == 64
            log += f"\n# Command\n\n```\n{line}\n```\n\n# Output (exit status {status})\n\n```\n{out[-3000:]}```\n"
        return {"task": name, "solved": bool(check(d, base, answer, before)), "commands": commands,
                "usage_errors": usage_errors, "answer": answer[-400:]}
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model")
    ap.add_argument("--runs", type=int, default=1)
    ap.add_argument("--only", default="add,update,restore")
    a = ap.parse_args()
    reference = subprocess.run([VO, "reference"], capture_output=True, text=True).stdout
    port, tmp = git_server.start("a.test", os.path.join(GRAPH, "repos"), [])
    out = os.path.join(ROOT, "bench2", "results", "package-tasks.jsonl")
    try:
        with open(out, "a") as f:
            for run in range(a.runs):
                for name in a.only.split(","):
                    rec = run_task(name, f"https://127.0.0.1:{port}", a.model, reference)
                    rec["run"] = run
                    f.write(json.dumps(rec) + "\n")
                    f.flush()
                    print(f"{name} run {run}: {'solved' if rec['solved'] else 'NOT solved'}, {len(rec['commands'])} commands, "
                          f"{rec['usage_errors']} usage errors: {[c['command'] for c in rec['commands']]}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
