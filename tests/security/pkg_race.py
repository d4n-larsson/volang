#!/usr/bin/env python3
"""Spec 013 (H6): a process swapping a package file between its pinned and another version (atomic
renames) must never make unpinned code run: every run either prints the pinned result or stops with
E0701. Exit 1 when the other version ran.

  pkg_race.py [VO] [RUNS]   (RUNS default 1000)
"""
import os, subprocess, sys, tempfile, threading

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
GOOD = 'export fn cents(n: int) -> str {\n    return "good " + str(n);\n}\n'
EVIL = 'export fn cents(n: int) -> str {\n    return "EVIL " + str(n);\n}\n'
d = tempfile.mkdtemp()
pkg = os.path.join(d, "vendor", "money")
os.makedirs(pkg)
open(os.path.join(pkg, "format.vo"), "w").write(GOOD)
open(os.path.join(d, "main.vo"), "w").write(
    "import money.format;\n\nfn main() uses console {\n    print(format.cents(1234));\n}\n")
subprocess.run([VO, "package", "main.vo", "money", "vendor/money"], cwd=d, check=True, capture_output=True)
stop = False


def swap():
    for i in range(1 << 62):
        if stop:
            return
        tmp = os.path.join(d, "t%d" % (i & 1))
        open(tmp, "w").write(EVIL if i & 1 else GOOD)
        os.replace(tmp, os.path.join(pkg, "format.vo"))


threading.Thread(target=swap, daemon=True).start()
counts = {"good": 0, "E0701": 0, "EVIL": 0, "other": 0}
for _ in range(RUNS):
    r = subprocess.run([VO, "run", "main.vo"], cwd=d, capture_output=True, text=True)
    key = ("EVIL" if "EVIL" in r.stdout else "good" if r.stdout == "good 1234\n" and r.returncode == 0
           else "E0701" if "E0701" in r.stderr else "other")
    counts[key] += 1
    if key == "other":
        print(r.returncode, r.stdout, r.stderr[:300])
stop = True
print("pkg_race: %d runs: %s" % (RUNS, counts))
sys.exit(1 if counts["EVIL"] or counts["other"] else 0)
