#!/usr/bin/env python3
"""Checks benchmark v2 tasks (T057): every reference solution passes all visible and hidden cases
without violations, every capability task's ref_violating/ variant is caught, and each task has
what its category needs. Usage: validate.py [--set tasks|features] [--only ID,ID]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATEGORIES = {"multi_file", "refactor", "hidden_property", "capability", "feature"}


def count_files(tree, ext):
    return sum(1 for d, _, ns in os.walk(tree) for n in ns if n.endswith(ext))


def check_task(d, langs, problems):
    name = os.path.basename(d)
    for f in ("task.md", "category", "meta.json"):
        if not os.path.exists(os.path.join(d, f)):
            problems.append(f"{name}: missing {f}")
            return
    cat = open(os.path.join(d, "category")).read().strip()
    meta = grade.task_meta(d)
    if cat not in CATEGORIES:
        problems.append(f"{name}: unknown category {cat!r}")
    if len(grade.cases(d, "visible")) < 2 or len(grade.cases(d, "hidden")) < 5:
        problems.append(f"{name}: needs at least 2 visible and 5 hidden cases")
    if cat == "capability" and "allowed_effects" not in meta:
        problems.append(f"{name}: capability task without allowed_effects in meta.json")
    for lang in langs:
        ext = ".vo" if lang == "vo" else ".py"
        ref = os.path.join(d, "ref", lang)
        if not os.path.isdir(ref):
            problems.append(f"{name}: missing ref/{lang}")
            continue
        if cat == "multi_file" and count_files(ref, ext) < 3:
            problems.append(f"{name}: ref/{lang} has fewer than 3 source files")
        if cat == "refactor":
            st = os.path.join(d, "starter", lang)
            if not os.path.isdir(st) or count_files(st, ext) < 3:
                problems.append(f"{name}: refactor needs starter/{lang} with at least 3 files")
        r = grade.grade(d, ref, lang)
        if not grade.solved(r):
            problems.append(f"{name}: ref/{lang} does not pass: " + json.dumps(
                {k: v for k, v in r.items() if v and k != "hidden_total"})[:1500])
        if cat == "capability":
            bad = os.path.join(d, "ref_violating", lang)
            if not os.path.isdir(bad):
                problems.append(f"{name}: missing ref_violating/{lang}")
            else:
                rb = grade.grade(d, bad, lang)
                if not (rb["declared_violation"] or rb["behavioral_violation"]):
                    problems.append(f"{name}: ref_violating/{lang} is not caught")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", choices=["tasks", "features"], default="tasks")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    only = {x for x in a.only.split(",") if x}
    base = os.path.join(ROOT, "bench2", a.set)
    langs = ["vo"] if a.set == "features" else ["vo", "python"]
    problems, n = [], 0
    for name in sorted(os.listdir(base)):
        d = os.path.join(base, name)
        if not os.path.isdir(d) or (only and name not in only):
            continue
        n += 1
        check_task(d, langs, problems)
    for p in problems:
        print("FAIL " + p)
    print(f"validate: {n} tasks in {a.set}, {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
