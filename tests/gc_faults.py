#!/usr/bin/env python3
"""The fault-injection matrix (spec 003 SC-004, T030). Usage: tests/gc_faults.py [KIND...]

For each kind, `make FAULT=<KIND>` builds a collector that skips one kind of root or object scan
(src/gc.c, `#ifdef GC_FAULT_<KIND>`). The stress suite must notice: `tests/run.sh --stress 1` with
that binary must fail at least one case, first over tests/gc/fault_targets, then (only if nothing
failed there) over tests/gc/edge and tests/cases/run, func and generic.

SC-004's kinds and the fault that removes each: locals -> VSTACK, captured values -> CLOSURE,
list elements -> LISTBUF, map entries -> MAPARR, record fields -> RECORD, variant payloads ->
VARIANT, builtin temporaries -> ROOTS (the C root stack; spec 008 removed
native-stack scanning, research R4), error or failure data -> GLOBALS. PERM (runtime caches)
is extra: nothing on the heap is reachable only from the permanent arena today, so its row is
informational and does not fail the script.
"""
import os
import re
import subprocess
import functools
import sys

print = functools.partial(print, flush=True)  # one line per kind as soon as it is known

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KINDS = ["VSTACK", "ROOTS", "GLOBALS", "LISTBUF", "MAPARR", "RECORD", "VARIANT", "CLOSURE", "PERM"]
INFORMATIONAL = {"PERM"}
WIDER = ["tests/gc/edge", "tests/cases/run", "tests/cases/func", "tests/cases/generic"]


def failing(binary, paths):
    env = dict(os.environ, VO_STRESS_BIN=binary)
    r = subprocess.run(["sh", "tests/run.sh", "--stress", "1"] + paths, cwd=ROOT, env=env,
                       capture_output=True, text=True, errors="replace", timeout=7200)  # poisoned output
    return re.findall(r"^FAIL (\S+)", r.stdout, re.M)


def caught(kind, paths):
    """Cases that fail with the fault build but pass with the plain stress build (a case too slow
    for a collection at every allocation fails both ways and proves nothing)."""
    fails = failing(os.path.join(ROOT, f"build-fault-{kind}", "vo"), paths)
    if not fails:
        return []
    plain = set(failing(os.path.join(ROOT, "build-stress", "vo"), [os.path.join(ROOT, f) for f in fails]))
    return [f for f in fails if f not in plain]


def main():
    kinds = sys.argv[1:] or KINDS
    missed = 0
    for kind in kinds:
        b = subprocess.run(["make", "-s", f"FAULT={kind}"], cwd=ROOT, capture_output=True, text=True)
        if b.returncode != 0:
            print(f"FAIL {kind}: build failed\n{b.stdout}{b.stderr}")
            missed += 1
            continue
        fails = caught(kind, ["tests/gc/fault_targets"])
        where = "fault_targets"
        if not fails:
            fails = caught(kind, WIDER)
            where = "corpus"
        if fails:
            print(f"ok   {kind}: caught by {len(fails)} case(s) in {where}, first {fails[0]}")
        elif kind in INFORMATIONAL:
            print(f"info {kind}: no case depends on it (nothing on the heap is reachable only from there)")
        else:
            print(f"FAIL {kind}: no case noticed the missing scan")
            missed += 1
    print("gc_faults: " + ("every kind is caught" if not missed else f"{missed} kind(s) not caught"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
