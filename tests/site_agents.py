#!/usr/bin/env python3
"""Spec 015 (SC-004): can an agent go from nothing to a running, tested program with an effect, given only
the Getting started pages? Runs the isolated protocol (bench2/agent.py: `claude -p` with no tools, an empty
working directory, no user settings) RUNS times; each reply is checked with `vo check`, `vo test` and
`vo run`. Costs agent time: run when asked; results go to specs/015-docs-site/results.md.

  site_agents.py [--runs 3] [--model M]
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "bench2"))
import agent  # noqa: E402

VO = os.path.join(ROOT, "bin", "vo")
PAGES = ["install", "first-program", "tests-and-effects"]
TASK = ("Using only the documentation above, write a vo program `main.vo` that reads a list of integers from "
        "the file `data/numbers.txt` (one per line), prints their sum, and has a `test` for the summing "
        "function. Declare exactly the effects it needs. Reply with the program in one fenced code block whose "
        "first line is `# file: main.vo`.")


def main():
    a = sys.argv[1:]
    runs = int(a[a.index("--runs") + 1]) if "--runs" in a else 3
    model = a[a.index("--model") + 1] if "--model" in a else None
    docs = "\n\n".join(open(os.path.join(ROOT, "site", "docs", "start", p + ".md"), encoding="utf-8").read() for p in PAGES)
    passed = 0
    for i in range(runs):
        reply = agent.ask("This is the Getting started documentation of the vo programming language:\n\n" + docs
                          + "\n\n" + TASK, model)
        program = agent.parse_reply(reply, "main.vo").get("main.vo", "")
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "data"))
            open(os.path.join(d, "data", "numbers.txt"), "w").write("1\n2\n39\n")
            open(os.path.join(d, "main.vo"), "w").write(program)
            check = subprocess.run([VO, "check", "main.vo"], cwd=d, capture_output=True, text=True)
            test = subprocess.run([VO, "test", "main.vo"], cwd=d, capture_output=True, text=True)
            run = subprocess.run([VO, "run", "main.vo"], cwd=d, capture_output=True, text=True)
        ok = check.returncode == 0 and test.returncode == 0 and "42" in run.stdout
        passed += ok
        print(f"run {i + 1}: {'pass' if ok else 'FAIL'} (check {check.returncode}, test {test.returncode}, "
              f"run printed {run.stdout.strip()[:40]!r})")
    print(f"site_agents: {passed} of {runs} runs reached a passing test and the right output on the first attempt")
    return 0 if passed == runs else 1


if __name__ == "__main__":
    sys.exit(main())
