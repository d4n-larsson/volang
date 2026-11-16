#!/usr/bin/env python3
"""Checks docs/reference.md against the toolchain (FR-030, SC-002).

- `vo reference` prints the document unchanged (and `vo reference --json` carries it as text)
- every ```vo example runs (followed by ```output), fails with a code (followed by ```error E####),
  or checks clean; consecutive blocks starting with `# file: name.vo` form one multi-file program
- ```vo-test blocks run with `vo test` against the following ```output (human format, ms normalized)
- ```vo-fmt blocks are formatted and compared with the following ```vo block
- a ```console block after a ```vo example runs each `$ vo ...` line in the example's directory
  and compares its stdout with the lines below it (spec 006: query, impact, edit)
- every top-level ## section has at least one example
- the document fits the token budget: words x 1.3 <= 15,000
Commands that are not implemented yet (exit 125) are reported as pending, not as failures.
"""
import json
import os
import re
import subprocess
import sys

from doc_examples import check_text

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
DOC = os.path.join(ROOT, "docs", "reference.md")
BUDGET = 15000

FENCE = re.compile(r"^```([\w-]*)(?: (\S+))?\n(.*?)^```\n", re.M | re.S)


def vo(args, cwd, stdin=None):
    return subprocess.run([VO] + args, capture_output=True, cwd=cwd, timeout=60,
                          input=stdin if stdin is not None else b"")


def main():
    problems, pending = [], []
    doc = open(DOC, "rb").read()
    out = vo(["reference"], ROOT).stdout
    if out != doc:
        problems.append("`vo reference` output differs from docs/reference.md (rebuild with make?)")
    rec = vo(["reference", "--json"], ROOT).stdout.decode("utf-8", "replace").strip()
    try:
        if json.loads(rec).get("text") != doc.decode("utf-8"):
            problems.append("`vo reference --json` text differs from docs/reference.md")
    except ValueError:
        problems.append("`vo reference --json` is not one JSON record")
    text = doc.decode("utf-8")
    words = len(text.split())
    tokens = int(words * 1.3)
    if tokens > BUDGET:
        problems.append(f"reference is ~{tokens} tokens ({words} words), budget {BUDGET}")

    # Split into top-level sections and collect fenced blocks with their positions.
    sections = [(m.start(), m.group(1)) for m in re.finditer(r"^## (.+)$", text, re.M)]
    blocks = [(m.start(), m.group(1), m.group(2), m.group(3)) for m in FENCE.finditer(text)]
    for i, (start, title) in enumerate(sections):
        end = sections[i + 1][0] if i + 1 < len(sections) else len(text)
        if not any(start <= b[0] < end and b[1] in ("vo", "vo-test", "vo-fmt") for b in blocks):
            problems.append(f"section '{title}' has no example")

    examples, problems2, pending2 = check_text(text)
    problems += problems2
    pending += pending2
    for p in problems:
        print("FAIL", p)
    for p in pending:
        print("PENDING", p)
    print(f"ref_examples: {examples} examples, ~{tokens} tokens of {BUDGET}, {len(problems)} problems, {len(pending)} pending")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
