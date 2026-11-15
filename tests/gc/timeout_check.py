#!/usr/bin/env python3
"""SC-007: programs that spend their time inside one builtin call stop within 1 s after a 2 s
--timeout (spec 003 US4). Each tests/cases/run/timeout_in_*.vo takes well over 5 s without a limit."""
import glob
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
bad = 0
for path in sorted(glob.glob(os.path.join(ROOT, "tests", "cases", "run", "timeout_in_*.vo"))):
    args = [w for w in open(path[:-3] + ".cmd").read().split() if w != "{}"]  # the case's own options
    if "--timeout" in args:
        args[args.index("--timeout") + 1] = "2"
    else:
        args += ["--timeout", "2"]
    t0 = time.monotonic()
    r = subprocess.run([os.path.join(ROOT, "bin", "vo")] + args + [path], capture_output=True, timeout=60)
    took = time.monotonic() - t0
    out = (r.stdout + r.stderr).decode()
    ok = "R0009" in out and took <= 3.0
    bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {os.path.basename(path)}: stopped after {took:.2f} s (exit {r.returncode})")
print("timeout_check: " + ("all stopped in time" if not bad else f"{bad} failed"))
sys.exit(1 if bad else 0)
