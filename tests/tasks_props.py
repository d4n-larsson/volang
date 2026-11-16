#!/usr/bin/env python3
"""Spec 016 SC-004: concurrent_map gives what map gives, over generated inputs, under several seeds.

  tasks_props.py [--seeds N]   (default 10: seeds 1..N of tests/cases/tasks/props_equal_map.vo)

Each seed runs the file's properties (100 generated cases each, inputs changed by the tasks included);
the script reports the generated cases in total and fails on any property that does not pass.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))
FILE = os.path.join(ROOT, "tests", "cases", "tasks", "props_equal_map.vo")


def main():
    seeds = int(sys.argv[sys.argv.index("--seeds") + 1]) if "--seeds" in sys.argv else 10
    cases, bad = 0, 0
    for seed in range(1, seeds + 1):
        r = subprocess.run([VO, "test", "--json", "--seed", str(seed), FILE], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            rec = json.loads(line)
            if rec.get("kind") != "test":
                continue
            cases += rec.get("cases", 0)
            if rec["status"] != "pass":
                bad += 1
                print(f"seed {seed}: {rec['name']}: {rec['status']} {json.dumps(rec.get('failure'))[:300]}")
    print(f"tasks_props: {seeds} seeds, {cases} generated cases, {bad} failing properties")
    return 1 if bad or cases < 1000 else 0


if __name__ == "__main__":
    sys.exit(main())
