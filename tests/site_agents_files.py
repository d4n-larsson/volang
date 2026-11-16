#!/usr/bin/env python3
"""Spec 015 (FR-011, specs/015-docs-site/contracts/agents.md), on the built site (make site-check):
llms.txt lists every page once with its summary, every link resolves to a Markdown copy, and llms-full.txt
starts with the version and holds every page."""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, "build", "site")
problems = []
llms = open(os.path.join(SITE, "llms.txt"), encoding="utf-8").read()
full = open(os.path.join(SITE, "llms-full.txt"), encoding="utf-8").read()
links = re.findall(r"^- \[[^\]]+\]\(([^)]+)\): (.*)$", llms, re.M)
if not llms.startswith("# vo\n\n> "):
    problems.append("llms.txt does not start with `# vo` and a summary")
seen = set()
for link, summary in links:
    if link in seen:
        problems.append(f"llms.txt lists {link} twice")
    seen.add(link)
    if not os.path.exists(os.path.join(SITE, link)):
        problems.append(f"llms.txt links {link}, which does not exist")
    if not summary.strip():
        problems.append(f"llms.txt: {link} has no summary")
copies = {os.path.relpath(p, SITE) for p in glob.glob(os.path.join(SITE, "**", "*.md"), recursive=True)}
for c in sorted(copies - seen):
    problems.append(f"{c} is not listed in llms.txt")
if not re.match(r"# vo documentation \(vo [^,]+, commit \w+\)", full):
    problems.append("llms-full.txt does not start with the version line")
for link, _ in links:
    addr = link[:-len("index.md")] if link.endswith("index.md") else link[:-3] + "/"
    if f"<!-- /{addr} -->" not in full:
        problems.append(f"llms-full.txt is missing {link}")
for pr in problems:
    print("FAIL", pr)
print(f"site_agents_files: {len(links)} pages listed, {len(copies)} Markdown copies, {len(problems)} problems")
sys.exit(1 if problems else 0)
