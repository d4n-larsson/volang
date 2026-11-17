#!/usr/bin/env python3
"""Spec 018 SC-002: `vo test --json` writes only JSON on stdout, and every test record that did not
pass carries a cause from the documented list.

Checks the expected outputs of the golden cases (tests/cases/**/*.out whose .cmd runs `test --json`,
or whose area runs tests as JSON by default) and of step cases (expected/out, lines of `vo test --json`
steps), so the check costs no runs.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "tests", "cases")
CAUSES = {"check_failed", "too_few_inputs", "error_in_code", "error_in_test", "requires_broken_by_test",
          "requires_broken_by_code", "ensures_broken", "hole", "outside_grant", "timeout", "out_of_memory"}


def check_records(lines, where, problems, stream=True):
    for n, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            if stream:
                problems.append(f"{where}:{n}: not JSON: {line[:80]!r}")
            continue
        if isinstance(rec, dict) and rec.get("kind") == "test" and rec.get("status") != "pass" and rec.get("cause") not in CAUSES:
            problems.append(f"{where}:{n}: a {rec.get('status')} record without a documented cause")


def main():
    problems, outs, steps = [], 0, 0
    for d, _, names in os.walk(CASES):
        for name in names:
            path = os.path.join(d, name)
            if name.endswith(".out") and not d.endswith("expected"):
                base = path[:-4]
                cmd = open(base + ".cmd").read() if os.path.exists(base + ".cmd") else ""
                default_json = not cmd and os.path.relpath(d, CASES).split(os.sep)[0] == "test"
                lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
                if (cmd.split()[:1] == ["test"] and "--json" in cmd.split()) or default_json:
                    outs += 1
                    check_records(lines, os.path.relpath(path, ROOT), problems)
                else: # any other output that holds test records: the causes only
                    check_records(lines, os.path.relpath(path, ROOT), problems, stream=False)
            elif name == "out" and d.endswith("expected"):
                steps += 1
                text = open(path, encoding="utf-8", errors="replace").read()
                for block in text.split("$ vo ")[1:]: # one block per vo step: its stdout comes first
                    head, _, rest = block.partition("\n")
                    if head.split()[:1] == ["test"] and "--json" in head.split():
                        check_records(rest.partition("--- stderr")[0].splitlines(), os.path.relpath(path, ROOT), problems)
    for p in problems:
        print("FAIL", p)
    print(f"json_stream: {outs} golden outputs, {steps} step outputs, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
