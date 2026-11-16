#!/usr/bin/env python3
"""Spec 014 SC-001: the property tests of every structure (tests/cases/collections/props_*.vo) under
10 seeds, 100 generated cases each: at least 1,000 generated operation sequences per property. With
--stress, also under the stress build, which checks every structure's invariants (AVL heights,
balance, order, size; heap order) after every change.

  structures_props.py [--stress]
"""
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINS = [os.path.join(ROOT, "build", "vo")]
if "--stress" in sys.argv:
    BINS.append(os.path.join(ROOT, "build-stress", "vo"))

bad = cases = 0
for vo in BINS:
    for path in sorted(glob.glob(os.path.join(ROOT, "tests", "cases", "collections", "props_*.vo"))):
        for seed in range(1, 11):
            r = subprocess.run([vo, "test", "--seed", str(seed), os.path.basename(path)], cwd=os.path.dirname(path),
                               capture_output=True, text=True, timeout=600)
            cases += sum(int(n) for n in re.findall(r"\((\d+) cases", r.stdout))
            if r.returncode != 0:
                bad += 1
                print(f"FAIL {os.path.basename(vo)} {os.path.basename(path)} --seed {seed}\n{r.stdout}{r.stderr}")
print(f"structures_props: {cases} generated cases, {bad} failing runs")
sys.exit(1 if bad else 0)
