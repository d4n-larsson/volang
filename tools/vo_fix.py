#!/usr/bin/env python3
"""Applies vo's fixes in place (spec 056): `vo check --json` on each entry program, then the first fix of
every diagnostic, once per round, up to 3 rounds while fixes remain. When the only errors left are E0504
(an effects record to approve, which has no fix), runs `vo effects --update` for that entry.

  tools/vo_fix.py [--only CODE,...] PATH...    a .vo entry, or a directory (each `main.vo` under it, else each
                                              .vo file); --only: apply only those codes' fixes

Prints the codes it fixed and the errors left; exit status 1 when errors are left.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
sys.path.insert(0, os.path.join(ROOT, "tests"))
from apply_fixes import offset  # noqa: E402


def check(entry):
    r = subprocess.run([VO, "check", "--json", os.path.basename(entry)], cwd=os.path.dirname(entry),
                       capture_output=True, timeout=300)
    recs = [json.loads(l) for l in r.stdout.decode("utf-8", "replace").splitlines() if l.startswith("{")]
    return [d for d in recs if d.get("kind") == "diagnostic"]


def apply(entry, diags):
    """The first fix of each diagnostic, per file, later edits first; overlapping ones keep the earlier."""
    base = os.path.dirname(entry)
    by_file = {}
    for d in diags:
        if d.get("fixes"):
            f = d["fixes"][0]
            by_file.setdefault(f["file"], []).append(f)
    applied = 0
    for name, fixes in by_file.items():
        path = os.path.join(base, name)
        data = open(path, "rb").read()
        spans = []
        for f in fixes:
            s, e = offset(data, f["start"]["line"], f["start"]["col"]), offset(data, f["end"]["line"], f["end"]["col"])
            if all(e <= a or s >= b for a, b, _ in spans):
                spans.append((s, e, f["replacement"].encode("utf-8")))
        for s, e, text in sorted(spans, reverse=True):
            data = data[:s] + text + data[e:]
            applied += 1
        open(path, "wb").write(data)
    return applied


def fix_entry(entry, only=None):
    """(codes fixed, errors left as 'CODE file:line message'); only: a set of codes whose fixes apply."""
    fixed = set()
    for _ in range(3):
        diags = check(entry)
        with_fix = [d for d in diags if d.get("fixes") and (not only or d["code"] in only)]
        if not with_fix:
            break
        fixed |= {d["code"] for d in with_fix}
        apply(entry, with_fix)
    errors = [d for d in check(entry) if d["severity"] == "error"]
    if only:
        return fixed, [f"{d['code']} {d['file']}:{d['start']['line']} {d['message']}" for d in errors if d["code"] in only]
    if errors and all(d["code"] == "E0504" for d in errors):
        subprocess.run([VO, "effects", "--update", os.path.basename(entry)], cwd=os.path.dirname(entry),
                       capture_output=True, timeout=300)
        fixed.add("E0504")
        errors = [d for d in check(entry) if d["severity"] == "error"]
    return fixed, [f"{d['code']} {d['file']}:{d['start']['line']} {d['message']}" for d in errors]


def entries(path):
    if os.path.isfile(path):
        return [path]
    mains = [os.path.join(d, "main.vo") for d, _, ns in os.walk(path) if "main.vo" in ns]
    if mains:
        return sorted(mains)
    return sorted(os.path.join(d, n) for d, _, ns in os.walk(path) for n in ns if n.endswith(".vo"))


def fix_paths(paths, only=None):
    """Fixes every entry under paths: (codes fixed, errors left)."""
    fixed, left = set(), []
    for p in paths:
        for e in entries(p):
            f, l = fix_entry(os.path.abspath(e), only)
            fixed |= f
            left += l
    return fixed, left


if __name__ == "__main__":
    args = sys.argv[1:]
    only = None
    if args[:1] == ["--only"]:
        only, args = set(args[1].split(",")), args[2:]
    fixed, left = fix_paths(args, only)
    print("fixed: " + (", ".join(sorted(fixed)) or "nothing"))
    for l in left:
        print("left: " + l)
    sys.exit(1 if left else 0)
