#!/usr/bin/env python3
"""Runs test scripts side by side: `make test`'s scripts, $VO_JOBS at a time (default: the number of cores).

    python3 tests/run_scripts.py "fuzz.py --iterations 200 --seed 1" apply_fixes.py ...

Each argument is a script under tests/ with its options. A script that does not exist yet is skipped, as
before. Every script runs to its end: one line per script as it finishes, then the output of each one that
failed, and exit status 1 if any did. The scripts do not share files (each makes its own directory under
build/tmp) or ports, so their order does not matter; the slowest start first. Scripts that time
themselves against fixed limits (ALONE) run after the others, one at a time, on an idle machine.
"""
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLOW = ["package_fetch.py", "package_graphs.py", "steps.py", "fmt_corpus.py", "workers.py", "migrate.py", "doc_conformance.py",
        "lsp.py", "decimals.py"]  # ponytail: measured 2026-11-26; reorder if it drifts
ALONE = ["workers.py", "stream.py", "bits.py", "iteration.py"]  # their speed checks fail under a full machine


def run(spec):
    start = time.monotonic()
    p = subprocess.run([sys.executable] + [os.path.join(ROOT, "tests", spec.split()[0])] + spec.split()[1:],
                       cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return spec, p.returncode, p.stdout.decode("utf-8", "replace"), time.monotonic() - start


def main():
    specs = []
    for spec in sys.argv[1:]:
        if os.path.isfile(os.path.join(ROOT, "tests", spec.split()[0])):
            specs.append(spec)
        else:
            print(f"== skip tests/{spec.split()[0]} (not created yet)")
    specs.sort(key=lambda s: SLOW.index(s.split()[0]) if s.split()[0] in SLOW else len(SLOW))
    jobs = int(os.environ.get("VO_JOBS") or os.cpu_count() or 1)
    failed = []

    def report(result):
        spec, code, out, secs = result
        last = out.strip().splitlines()[-1] if out.strip() else ""
        print(f"{'ok  ' if code == 0 else 'FAIL'} {spec} ({secs:.0f} s) {last[:120]}", flush=True)
        if code:
            failed.append((spec, out))

    alone = [s for s in specs if s.split()[0] in ALONE]
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for f in as_completed([pool.submit(run, s) for s in specs if s not in alone]):
            report(f.result())
    for s in alone:
        report(run(s))
    for spec, out in failed:
        print(f"\n== {spec} failed:\n{out}")
    print(f"scripts: {len(specs) - len(failed)} passed, {len(failed)} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
