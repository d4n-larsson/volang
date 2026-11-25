#!/usr/bin/env python3
"""Spec 056 (SC-001): no two std modules export the same name, so any two library items can be imported
together. Lists every exported item of every std module (`vo doc --json`), fails on a name exported by two
modules, and checks a program importing one item of each name (no E0202)."""
import glob
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")


def main():
    owners = {}
    modules = sorted(os.path.basename(p)[:-3] for p in glob.glob(os.path.join(ROOT, "std", "*.vo")))
    for m in modules:
        r = subprocess.run([VO, "doc", "--json", "std." + m], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            rec = json.loads(line)
            if rec.get("kind") == "doc" and rec.get("name"):
                owners.setdefault(rec["name"], []).append(m)
    problems = [f"`{n}` is exported by {', '.join('std.' + m for m in ms)}" for n, ms in sorted(owners.items())
                if len(set(ms)) > 1]
    d = tempfile.mkdtemp(prefix="vo-names-")
    src = "".join(f"import std.{ms[0]}.{n};\n" for n, ms in sorted(owners.items()) if len(set(ms)) == 1)
    with open(os.path.join(d, "all.vo"), "w") as f:
        f.write(src + "\nfn main() {\n}\n")
    r = subprocess.run([VO, "check", "--json", "all.vo"], cwd=d, capture_output=True, text=True)
    for line in r.stdout.splitlines():
        rec = json.loads(line)
        if rec.get("kind") == "diagnostic" and rec["severity"] == "error":
            problems.append(f"importing every item: {rec['code']} {rec['message']}")
    for p in problems:
        print("FAIL", p)
    print(f"std_names: {len(owners)} names in {len(modules)} modules, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
