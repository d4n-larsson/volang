#!/usr/bin/env python3
"""Benchmark v2 metrics (latest record per task and language).

SC-001: not at the ceiling: a language below 85% first-attempt success overall, and both below 95%
        in at least two categories.
SC-002: vo solves at least as many tasks as Python within three attempts, and needs at least 20%
        fewer attempts on average or solves at least 10 points more (unsolved counts as 4 attempts).
SC-003: capability tasks: every vo attempt that exceeded its allowance was caught (by construction:
        grading rejects it); Python's violations are counted for comparison.
--set features: SC-006: vo solves >= 90% within three attempts and >= 75% on the first.
"""
import argparse
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load(name):
    path = os.path.join(ROOT, "bench2", "results", name + ".jsonl")
    latest = {}
    if os.path.exists(path):
        for line in open(path):
            r = json.loads(line)
            latest[r["task"]] = r
    return latest


def stats(recs):
    n = len(recs)
    if not n:
        return None
    first = sum(1 for r in recs if r["solved"] and r["attempts"] == 1)
    within = sum(1 for r in recs if r["solved"])
    mean = sum(r["attempts"] if r["solved"] else 4 for r in recs) / n
    hp = sum(r["hidden_passed"] for r in recs)
    ht = sum(r["hidden_total"] for r in recs) or 1
    return {"n": n, "first": first / n, "within": within / n, "solved": within, "mean": mean,
            "hidden": hp / ht, "declared": sum(r.get("declared_violation", False) for r in recs),
            "behavioral": sum(r.get("behavioral_violation", False) for r in recs)}


def show(label, s):
    if not s:
        print(f"  {label:16} (no results)")
        return
    print(f"  {label:16} n={s['n']:3}  first {s['first']:6.1%}  within 3 {s['within']:6.1%}  "
          f"mean attempts {s['mean']:.2f}  hidden {s['hidden']:6.1%}  "
          f"violations declared {s['declared']} behavioral {s['behavioral']}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", choices=["tasks", "hard", "features"], default="tasks")
    a = ap.parse_args()
    if a.set == "features":
        vo = list(load("vo-features").values())
        s = stats(vo)
        show("vo features", s)
        if s:
            ok = s["within"] >= 0.90 and s["first"] >= 0.75
            print(f"SC-006: {'PASS' if ok else 'FAIL'} (within 3: {s['within']:.1%} >= 90%, first: {s['first']:.1%} >= 75%)")
        return
    suffix = "-hard" if a.set == "hard" else ""
    data = {lang: list(load(lang + suffix).values()) for lang in ("vo", "python")}
    cats = sorted({r["category"] for recs in data.values() for r in recs})
    for lang, recs in data.items():
        print(lang)
        show("overall", stats(recs))
        for c in cats:
            show(c, stats([r for r in recs if r["category"] == c]))
    vo, py = stats(data["vo"]), stats(data["python"])
    if not vo or not py:
        return
    below = [c for c in cats if all((stats([r for r in data[l] if r["category"] == c]) or {"first": 0})["first"] < 0.95
                                    for l in data)]
    sc1 = min(vo["first"], py["first"]) < 0.85 and len(below) >= 2
    print(f"SC-001: {'PASS' if sc1 else 'FAIL'} (lowest first-attempt {min(vo['first'], py['first']):.1%} < 85%; "
          f"categories where both are below 95%: {below})")
    fewer = (py["mean"] - vo["mean"]) / py["mean"] if py["mean"] else 0
    sc2 = vo["solved"] >= py["solved"] and (fewer >= 0.20 or vo["within"] - py["within"] >= 0.10)
    print(f"SC-002: {'PASS' if sc2 else 'FAIL'} (solved vo {vo['solved']} / python {py['solved']}; "
          f"mean attempts vo {vo['mean']:.2f} / python {py['mean']:.2f} ({fewer:+.0%}); "
          f"within 3 vo {vo['within']:.1%} / python {py['within']:.1%})")
    cap_vo = [r for r in data["vo"] if r["category"] == "capability"]
    cap_py = [r for r in data["python"] if r["category"] == "capability"]
    print(f"SC-003: vo capability tasks with a violation in the final attempt: "
          f"{sum(r['declared_violation'] or r['behavioral_violation'] for r in cap_vo)} (all graded as failed); "
          f"python: {sum(r['behavioral_violation'] for r in cap_py)} of {len(cap_py)}")


if __name__ == "__main__":
    main()
