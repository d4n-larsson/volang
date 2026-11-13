#!/usr/bin/env python3
"""Freezes the v1 corpus for SC-005 (T002). Run once, with build-v1/vo built from the v1 sources.

Usage: snapshot.py SRC_ROOT. SRC_ROOT must hold the *v1* tests/cases and bench/tasks (the in-repo
ones are migrated to v2 and no longer check under v1); refuses to run on the repository itself.

Entries: golden cases (a .vo with a sibling .cmd/.out/.err/.code) that v1 `check` accepts and
that have `fn main(` (recorded with `run`) or tests (recorded with human `test`), plus every
bench/tasks/*/solution.vo. A case inside a subdirectory of its area (multi-file) is copied with
its whole directory so the imports come along. Writes tests/migrate/v1/<id>/..., <entry>.out,
<entry>.v1code, <entry>.in (if any), <entry>.mode, <entry>.v1diags (diagnostic codes v1 reported),
<entry>.v1fmt (whether v1 `fmt --check` accepted it), and the sorted MANIFEST.
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
V1 = os.path.join(ROOT, "build-v1", "vo")
OUT = os.path.join(ROOT, "tests", "migrate", "v1")
SRC = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else None
CASES = os.path.join(SRC or ROOT, "tests", "cases")


def vo(args, cwd, stdin):
    env = dict(os.environ, VO_BIN=V1)
    return subprocess.run([os.path.join(ROOT, "bin", "vo")] + args, cwd=cwd, input=stdin,
                          capture_output=True, timeout=60, env=env)


def norm(b):
    return re.sub(rb"\((\d+)ms\)", b"(0ms)", b)


def candidates():
    for d, _, files in os.walk(CASES):
        if os.path.relpath(d, CASES).split(os.sep)[0] in ("effects", "func", "generic"):
            continue                    # v2 areas, not part of the v1 corpus
        for f in files:
            if not f.endswith(".vo") or f.endswith(".expected.vo"):
                continue
            base = os.path.join(d, f[:-3])
            if any(os.path.exists(base + s) for s in (".cmd", ".out", ".err", ".code")):
                yield os.path.join(d, f)
    for t in sorted(os.listdir(os.path.join(SRC, "bench", "tasks"))):
        p = os.path.join(SRC, "bench", "tasks", t, "solution.vo")
        if os.path.exists(p):
            yield p


def main():
    if not SRC or SRC == ROOT:
        sys.exit("usage: snapshot.py V1_SOURCE_ROOT (a copy holding the v1 tests/cases and bench/tasks)")
    if not os.path.exists(V1):
        sys.exit("build-v1/vo is missing: run `make BUILD=build-v1` on the v1 sources first")
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(OUT)
    manifest = []
    for src in sorted(candidates()):
        text = open(src, encoding="utf-8", errors="replace").read()
        if "fn main(" in text:
            mode = "run"
        elif re.search(r'^\s*test "', text, re.M):
            mode = "test"
        else:
            continue
        rel = os.path.relpath(src, SRC)
        parts = rel.split(os.sep)
        # tests/cases/<area>/<dir>/main.vo -> copy <dir>; otherwise copy just the file
        if parts[:2] == ["tests", "cases"] and len(parts) > 4:
            srcdir = os.path.join(SRC, *parts[:4])
            ident = "-".join(parts[2:4])
            entry_rel = os.path.join(*parts[4:])
        else:
            srcdir = None
            ident = "-".join(p for p in parts[-3:-1] if p not in ("cases",)) + "-" + parts[-1][:-3]
            entry_rel = parts[-1]
        if vo(["check", src], os.path.dirname(src), b"").returncode != 0:
            continue
        dest = os.path.join(OUT, ident)
        if srcdir:
            if os.path.exists(dest):
                continue
            shutil.copytree(srcdir, dest)
        else:
            os.makedirs(dest, exist_ok=True)
            shutil.copy(src, os.path.join(dest, entry_rel))
            stdin_src = src[:-3] + ".in"
            if not os.path.exists(stdin_src):  # bench solutions: their first case
                stdin_src = os.path.join(os.path.dirname(src), "cases", "1.in")
            if os.path.exists(stdin_src):
                shutil.copy(stdin_src, os.path.join(dest, entry_rel[:-3] + ".in"))
        entry = os.path.join(dest, entry_rel)
        stdin = open(entry[:-3] + ".in", "rb").read() if os.path.exists(entry[:-3] + ".in") else b""
        r = vo([mode, entry_rel], os.path.dirname(entry), stdin)
        base = entry[:-3]
        open(base + ".mode", "w").write(mode + "\n")
        open(base + ".v1out", "wb").write(norm(r.stdout))
        open(base + ".v1code", "w").write(f"{r.returncode}\n")
        diag = vo(["check", "--json", entry_rel], os.path.dirname(entry), b"").stdout.decode()
        codes = sorted(set(re.findall(r'"code":"([EW]\d{4})"', diag)))
        open(base + ".v1diags", "w").write(" ".join(codes) + "\n")
        files = [os.path.join(d, n) for d, _, ns in os.walk(os.path.dirname(entry)) for n in ns if n.endswith(".vo")]
        canon = vo(["fmt", "--check"] + files, os.path.dirname(entry), b"").returncode == 0
        open(base + ".v1fmt", "w").write("canonical\n" if canon else "not canonical\n")
        manifest.append(os.path.relpath(entry, OUT))
    open(os.path.join(OUT, "MANIFEST"), "w").write("\n".join(sorted(manifest)) + "\n")
    print(f"snapshot: {len(manifest)} entries")


if __name__ == "__main__":
    main()
