#!/usr/bin/env python3
"""Spec 018 SC-005: mutation testing finds the check a test suite is missing.

Each tests/mutate_eval/NAME/ holds a program with a known weak spot: weak.vo (tests that miss it),
full.vo (the same with the missing test added) and expect.txt (`LINE:COL text -> mutated`, the change
that only the full tests catch). The change must survive `vo test --mutate weak.vo` and be caught
with full.vo. Then every example with tests in examples/ is run with --mutate: each under 60 s.

  mutate_eval.py [--no-examples]
"""
import json
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))
EVAL = os.path.join(ROOT, "tests", "mutate_eval")


def survivors(path):
    p = subprocess.run([VO, "test", "--json", "--mutate", os.path.basename(path)], cwd=os.path.dirname(path),
                       capture_output=True, text=True, timeout=300)
    out = set()
    for line in p.stdout.splitlines():
        rec = json.loads(line)
        if rec.get("kind") == "mutant":
            out.add(f"{rec['start']['line']}:{rec['start']['col']} {rec['text']} -> {rec['mutated']}")
    return p.returncode, out


def main():
    bad = 0
    for name in sorted(os.listdir(EVAL)):
        d = os.path.join(EVAL, name)
        want = open(os.path.join(d, "expect.txt")).read().strip()
        rc_w, weak = survivors(os.path.join(d, "weak.vo"))
        rc_f, full = survivors(os.path.join(d, "full.vo"))
        ok = rc_w == 0 and rc_f == 0 and want in weak and want not in full
        bad += not ok
        print(f"mutate_eval: {name}: {'ok' if ok else 'FAILED'} ({want}; weak: {len(weak)} survivors, "
              f"full: {len(full)})")
    if "--no-examples" not in sys.argv:
        for entry in sorted(os.listdir(os.path.join(ROOT, "examples"))):
            d = os.path.join(ROOT, "examples", entry)
            main_vo = os.path.join(d, "src", "main.vo")
            if not os.path.isfile(main_vo) or "\ntest " not in "\n" + open(main_vo).read():
                continue
            start = time.monotonic()
            p = subprocess.run([VO, "test", "--json", "--mutate", "src/main.vo"], cwd=d, capture_output=True, text=True,
                               timeout=600)
            dt = time.monotonic() - start
            summary = [json.loads(l) for l in p.stdout.splitlines() if '"mutation_summary"' in l]
            s = summary[0] if summary else {}
            ok = p.returncode == 0 and summary and dt < 60
            bad += not ok
            print(f"mutate_eval: examples/{entry}: {dt:.1f} s, {s.get('mutants', '?')} changes, "
                  f"{s.get('caught', '?')} caught, {s.get('survived', '?')} survived, "
                  f"{s.get('not_tried', '?')} not tried{'' if ok else ' FAILED (exit ' + str(p.returncode) + ')'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
