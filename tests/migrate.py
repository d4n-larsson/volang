#!/usr/bin/env python3
"""v1 -> v2 migration through fixes (US5, SC-005, FR-033).

Default: for every entry in tests/migrate/v1/MANIFEST (the frozen v1 corpus), copy it, check it,
require that only migration diagnostics appear (E0501, plus W0504 for multi-file programs) besides
those v1 already reported, apply
the first fix of every diagnostic once, then require a clean check, canonical formatting
(`fmt --check`, for files v1 already accepted as canonical), and byte-identical stdout and exit status against the recorded v1 run.

--write PATH...: apply the first fix of every E0501 once to the given files in place (used to
migrate the in-repo corpus, T023). A directory means every .vo file under it.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CORPUS = os.path.join(ROOT, "tests", "migrate", "v1")
MIGRATION = {"E0501", "W0504"}


def vo(args, cwd, stdin=b""):
    return subprocess.run([VO] + args, cwd=cwd, input=stdin, capture_output=True, timeout=120)


def offset(data, line, col):
    """Byte offset of a 1-based line/column (columns count code points)."""
    lines = data.split(b"\n")
    off = sum(len(l) + 1 for l in lines[:line - 1])
    text = lines[line - 1].decode("utf-8", "replace") if line - 1 < len(lines) else ""
    return off + len(text[:col - 1].encode("utf-8"))


def diagnostics(entry):
    r = vo(["check", "--json", os.path.basename(entry)], os.path.dirname(entry))
    recs = [json.loads(l) for l in r.stdout.decode("utf-8", "replace").splitlines() if l.startswith("{")]
    return [d for d in recs if d.get("kind") == "diagnostic"]


def apply_first_fixes(entry, diags):
    """Applies the first fix of each diagnostic once; overlapping fixes keep the earlier one."""
    base = os.path.dirname(entry)
    by_file = {}
    for d in diags:
        if d.get("fixes"):
            f = d["fixes"][0]
            by_file.setdefault(f["file"], []).append(f)
    for name, fixes in by_file.items():
        path = os.path.join(base, name)
        data = open(path, "rb").read()
        spans = []
        for f in fixes:
            s = offset(data, f["start"]["line"], f["start"]["col"])
            e = offset(data, f["end"]["line"], f["end"]["col"])
            if any(not (e < a or s > b) for a, b, _ in spans):
                continue
            spans.append((s, e, f["replacement"].encode("utf-8")))
        for s, e, rep in sorted(spans, reverse=True):
            data = data[:s] + rep + data[e:]
        open(path, "wb").write(data)
    return sum(len(v) for v in by_file.values())


def write_mode(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            for d, _, names in os.walk(p):
                files += [os.path.join(d, n) for n in sorted(names) if n.endswith(".vo")]
        else:
            files.append(p)
    changed = 0
    for f in files:
        f = os.path.abspath(f)
        diags = [d for d in diagnostics(f) if d["code"] == "E0501"]
        if diags:
            apply_first_fixes(f, diags)
            changed += 1
            print(f"migrated {os.path.relpath(f, ROOT)} ({len(diags)} fixes)")
    print(f"migrate --write: {changed} of {len(files)} files changed")


def norm(b):
    return re.sub(rb"\((\d+)ms\)", b"(0ms)", b)


def verify():
    entries = open(os.path.join(CORPUS, "MANIFEST")).read().split()
    problems = []
    for rel in entries:
        top = rel.split("/")[0]
        with tempfile.TemporaryDirectory() as tmp:
            shutil.copytree(os.path.join(CORPUS, top), os.path.join(tmp, top))
            entry = os.path.join(tmp, rel)
            base = entry[:-3]
            diags = diagnostics(entry)
            v1codes = set(open(base + ".v1diags").read().split())
            other = sorted({d["code"] for d in diags} - MIGRATION - v1codes)
            if other:
                problems.append(f"{rel}: non-migration diagnostics {other}")
                continue
            apply_first_fixes(entry, diags)
            after = [d for d in diagnostics(entry) if d["severity"] == "error"]
            if after:
                problems.append(f"{rel}: still {len(after)} error(s) after one round: "
                                f"{[d['code'] + ' ' + d['message'] for d in after][:3]}")
                continue
            files = [os.path.join(d, n) for d, _, ns in os.walk(os.path.dirname(entry)) for n in ns
                     if n.endswith(".vo")]
            v1canon = open(base + ".v1fmt").read().strip() == "canonical"
            if v1canon and vo(["fmt", "--check"] + files, os.path.dirname(entry)).returncode != 0:
                problems.append(f"{rel}: not canonical after the fixes (fmt --check)")
                continue
            mode = open(base + ".mode").read().strip()
            stdin = open(base + ".in", "rb").read() if os.path.exists(base + ".in") else b""
            r = vo([mode, os.path.basename(entry)], os.path.dirname(entry), stdin)
            want_out = open(base + ".v1out", "rb").read()
            want_code = int(open(base + ".v1code").read())
            if norm(r.stdout) != want_out or r.returncode != want_code:
                problems.append(f"{rel}: behavior changed (exit {r.returncode}, expected {want_code})")
    for p in problems:
        print("FAIL " + p)
    print(f"migrated {len(entries) - len(problems)}/{len(entries)}")
    return 1 if problems else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--write":
        write_mode(sys.argv[2:])
    else:
        sys.exit(verify())
