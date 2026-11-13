#!/usr/bin/env python3
"""Benchmark metrics from bench/results/*.jsonl (latest record per task and language).

SC-001: vo solved on the first attempt >= 70% and within three attempts >= 95%.
SC-008: mean attempts per task lower for vo than for Python (an unsolved task counts as 4).
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load(lang):
    path = os.path.join(ROOT, "bench", "results", f"{lang}.jsonl")
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
    first = sum(1 for r in recs.values() if r["solved"] and r["attempts"] == 1)
    within = sum(1 for r in recs.values() if r["solved"])
    mean = sum(r["attempts"] if r["solved"] else 4 for r in recs.values()) / n
    return {"tasks": n, "first": first / n, "within3": within / n, "mean_attempts": mean}


def main():
    vo, py = stats(load("vo")), stats(load("python"))
    for name, s in (("vo", vo), ("python", py)):
        if s:
            print(f"{name}: {s['tasks']} tasks, first attempt {s['first']:.0%}, "
                  f"within 3 {s['within3']:.0%}, mean attempts {s['mean_attempts']:.2f}")
        else:
            print(f"{name}: no results")
    ok = True
    if vo:
        sc1 = vo["first"] >= 0.70 and vo["within3"] >= 0.95
        print(f"SC-001 ({'met' if sc1 else 'NOT met'}): first >= 70% and within three >= 95%")
        ok &= sc1
    if vo and py:
        sc8 = vo["mean_attempts"] < py["mean_attempts"]
        print(f"SC-008 ({'met' if sc8 else 'NOT met'}): vo {vo['mean_attempts']:.2f} vs python {py['mean_attempts']:.2f} mean attempts")
        ok &= sc8
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
