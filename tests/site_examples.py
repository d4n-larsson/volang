#!/usr/bin/env python3
"""Spec 015 (FR-007, FR-008): every example on the documentation site's hand-written pages
(site/docs/**/*.md) and in the examples' READMEs follows specs/015-docs-site/contracts/examples.md and is
checked: programs check, run and print what the page shows; error examples produce their code.

  site_examples.py [PAGE.md...]  the pages, or only the given ones (part of `make test`)
  site_examples.py --self-test   the deliberately broken fixture must fail, naming its page and line
  site_examples.py --coverage    (spec 015 T014) every reference section linked from the language guide;
                                 every error code a guide page mentions links to its catalog page
  site_examples.py --options     (T023) command pages' Options tables against the toolchain
  site_examples.py --start       (T009) the shell steps of Getting started on a fresh clone
"""
import glob
import os
import re
import subprocess
import sys
import tempfile

from doc_examples import ROOT, VO, check_text

PAGES = sorted(glob.glob(os.path.join(ROOT, "site", "docs", "**", "*.md"), recursive=True)) + \
    sorted(glob.glob(os.path.join(ROOT, "examples", "*", "README.md")))


def rel(p):
    return os.path.relpath(p, ROOT)


def check_pages(paths):
    total, problems, pending = 0, [], []
    for p in paths:
        n, probs, pend = check_text(open(p, encoding="utf-8").read(), rel(p))
        total += n
        problems += probs
        pending += pend
    return total, problems, pending


def self_test():
    fixture = os.path.join(ROOT, "tests", "site_examples_fixtures", "broken.md")
    n, problems, _ = check_pages([fixture])
    ok = n == 1 and len(problems) == 1 and problems[0].startswith(rel(fixture) + ":6")
    print(("ok" if ok else "FAIL") + " self-test: " + (problems[0].splitlines()[0] if problems else "no problem reported"))
    return 0 if ok else 1


def coverage():
    problems = []
    ref = open(os.path.join(ROOT, "docs", "reference.md"), encoding="utf-8").read()
    sections = re.findall(r"^## (.+)$", ref, re.M)
    guide = {p: open(p, encoding="utf-8").read() for p in glob.glob(os.path.join(ROOT, "site", "docs", "guide", "*.md"))}
    tools = [open(p, encoding="utf-8").read() for p in glob.glob(os.path.join(ROOT, "site", "docs", "tools", "*.md"))]
    text = "\n".join(list(guide.values()) + tools)
    for s in sections:
        slug = re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")
        if f"reference/{slug}" not in text:
            problems.append(f"reference section '{s}' (reference/{slug}) is not linked from the language or toolchain guide")
    for p, t in guide.items():
        for code in sorted(set(re.findall(r"\b([EWRT]\d{4})\b", t))):
            if f"errors/{code}" not in t:
                problems.append(f"{rel(p)}: mentions {code} without linking errors/{code}")
        for part in ("## ",):
            if part not in t:
                problems.append(f"{rel(p)}: no sections")
    for pr in problems:
        print("FAIL", pr)
    print(f"site_examples --coverage: {len(sections)} reference sections, {len(guide)} chapters, {len(problems)} problems")
    return 1 if problems else 0


def options():
    problems = []
    r = subprocess.run([VO, "check", "--no-such-option"], capture_output=True, text=True)
    listed = set(re.findall(r"(--[a-z][a-z-]*)", r.stdout + r.stderr)) - {"--no-such-option"}
    pages = {os.path.basename(p)[:-3]: open(p, encoding="utf-8").read()
             for p in glob.glob(os.path.join(ROOT, "site", "docs", "tools", "*.md"))}
    documented = {}
    for cmd, t in pages.items():
        m = re.search(r"^## Options\n(.*?)(?=^## |\Z)", t, re.M | re.S)
        if m:
            documented[cmd] = set(re.findall(r"^\| `(--[a-z][a-z-]*)", m.group(1), re.M))
    everywhere = set().union(*documented.values()) if documented else set()
    for o in sorted(listed - everywhere):
        problems.append(f"{o} (listed by the toolchain) is on no command page")
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "main.vo"), "w").write('fn main() uses console {\n    print("x");\n}\n')
        for cmd, opts in documented.items():
            for o in opts:
                r = subprocess.run([VO, cmd, o, "main.vo"], capture_output=True, text=True, cwd=d, timeout=60)
                if "Unknown option" in r.stdout + r.stderr:
                    problems.append(f"tools/{cmd}.md documents {o}, which `vo {cmd}` does not accept")
    for pr in problems:
        print("FAIL", pr)
    print(f"site_examples --options: {len(listed)} options listed, {len(documented)} command pages, {len(problems)} problems")
    return 1 if problems else 0


def start():
    page = os.path.join(ROOT, "site", "docs", "start", "install.md")
    text = open(page, encoding="utf-8").read()
    steps = re.findall(r"^```sh\n(.*?)^```\n(?:\n?```text\n(.*?)^```\n)?", text, re.M | re.S)
    problems = []
    with tempfile.TemporaryDirectory() as d:
        subprocess.run(["git", "clone", "-q", ROOT, os.path.join(d, "volang")], check=True)
        cwd = os.path.join(d, "volang")
        for cmds, expected in steps:
            for line in [l for l in cmds.splitlines() if l.strip() and not l.lstrip().startswith("#")]:
                if line.startswith("git clone") or line.startswith("cd volang"):
                    continue  # the clone above stands for these
                line = line.split("  #")[0]
                r = subprocess.run(line, shell=True, cwd=cwd, capture_output=True, text=True, timeout=900)
                if r.returncode != 0:
                    problems.append(f"`{line}` exits {r.returncode}: {r.stderr[-300:]}")
            if expected and problems == [] and not r.stdout.startswith(expected.split("\n")[0][:20]):
                problems.append(f"`{cmds.strip()}` printed {r.stdout[:200]!r}, the page shows {expected[:200]!r}")
    for pr in problems:
        print("FAIL", pr)
    print(f"site_examples --start: {len(steps)} steps, {len(problems)} problems")
    return 1 if problems else 0


def main():
    a = sys.argv[1:]
    if "--self-test" in a:
        return self_test()
    if "--coverage" in a:
        return coverage()
    if "--options" in a:
        return options()
    if "--start" in a:
        return start()
    only = [os.path.abspath(x) for x in a if x.endswith(".md")]  # check only these pages
    pages = only or PAGES
    n, problems, pending = check_pages(pages)
    for p in problems:
        print("FAIL", p)
    for p in pending:
        print("PENDING", p)
    print(f"site_examples: {len(pages)} pages, {n} examples, {len(problems)} problems")
    return 1 if problems or (not only and self_test()) else 0


if __name__ == "__main__":
    sys.exit(main())
