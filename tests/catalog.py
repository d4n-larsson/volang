#!/usr/bin/env python3
"""Checks the error catalog (docs/errors.md) against the toolchain, the tests and the contract.

- every section has a filled Example and Fixed
- each Example produces its code (check, run, or run for R codes); each Fixed checks and runs clean
- every E, W, R and T code (except T0003) appears in an expected output under tests/cases/
  (including the expected/ files of step cases)
- headings and Fixable lines match contracts/error-codes.md
"""
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CONTRACTS = [os.path.join(ROOT, "specs", "001-vo-lang-core", "contracts", "error-codes.md"),
             os.path.join(ROOT, "specs", "002-effects-functions-generics", "contracts", "error-codes-v2.md")]
# Codes whose trigger cannot be shown in a short program: content is checked, execution is not.
# E0115 needs nesting deep enough to exhaust a 64 MiB stack (golden case: check/nesting.vo).
# E0104 needs an invalid UTF-8 byte, which a UTF-8 doc block cannot hold (golden case: lex/invalid_utf8.vo).
NO_SHORT_EXAMPLE = {"E0115", "E0104", "R0014", "E0211"}  # R0014 needs vo replay, E0211 a link (step cases)


def sections():
    text = open(os.path.join(ROOT, "docs", "errors.md"), encoding="utf-8").read()
    out = []
    for m in re.finditer(r"^## ([EWRT]\d{4}) (\w+)\n(.*?)(?=^## |\Z)", text, re.M | re.S):
        out.append((m.group(1), m.group(2), m.group(3)))
    return out


def part(body, title):
    m = re.search(r"^### " + title + r"\n(.*?)(?=^### |\Z)", body, re.M | re.S)
    return m.group(1) if m else ""


def blocks(text):
    """```vo blocks, plus ```text blocks that hold a `# file: vo.effects` record or a
    `# file: vo.packages` manifest for the example."""
    return [b for lang, b in re.findall(r"```(vo|text)\n(.*?)```", text, re.S)
            if lang == "vo" or b.startswith(("# file: vo.effects", "# file: vo.packages"))]


REPEAT = re.compile(r"^(\s*)# \(the next line, (\d+) times: [A-Za-z]\w*?_(\d+) to [A-Za-z]\w*?_\d+\)\n(.*\n)", re.M)


def expand(block):
    """Spec 027: `# (the next line, K times: name_A to name_B)` stands for that line written K times with
    `_N` numbered from A, so an example can show a 400-line file or a 51-statement function briefly."""
    def rep(m):
        k, start, line = int(m.group(2)), int(m.group(3)), m.group(4)
        return "".join(line.replace("_N", f"_{start + i}") for i in range(k))
    return REPEAT.sub(rep, block)


def write_program(blks, d):
    """Writes code blocks into d; returns the entry path."""
    paths = []
    blks = [expand(b) for b in blks]
    for i, b in enumerate(blks):
        m = re.match(r"# file: (\S+)\n", b)
        unnamed = [x for x in blks if not x.startswith("# file:")]
        name = m.group(1) if m else ("main.vo" if len(unnamed) == 1 else f"part{i}.vo")
        path = os.path.join(d, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "w", encoding="utf-8").write(b)
        paths.append((path, "fn main" in b))
    return next((p for p, has_main in paths if has_main), paths[0][0])


def run(args, entry):
    p = subprocess.run([VO] + args + [entry], capture_output=True, stdin=subprocess.DEVNULL,
                       cwd=os.path.dirname(entry), timeout=30)
    recs = []
    for stream in (p.stdout, p.stderr):
        for line in stream.decode("utf-8", "replace").splitlines():
            if line.startswith("{"):
                try:
                    recs.append(json.loads(line))
                except ValueError:
                    pass
    return p.returncode, recs


def contract():
    out = {}
    for code, name, fix in re.findall(r"^\| ([EWRT]\d{4}) \| (\w+) \|(?: ([^|]*) \|)?",
                                      "\n".join(open(c, encoding="utf-8").read() for c in CONTRACTS), re.M):
        fix = (fix or "").strip()
        if code[0] in "RT" or not fix or fix.startswith("—"):
            fixable = "no"
        elif "when" in fix or "similar" in fix:
            fixable = "sometimes"
        else:
            fixable = "yes"
        out[code] = (name, fixable)
    for spec in ("005-holes-props-contracts", "006-query-edit-trace", "007-stdlib-packages", "009-small-fixes", "010-export-visibility", "012-spec-annotations", "013-security-fixes", "014-complex-data-structures", "016-concurrency", "017-filesystem-processes", "018-agent-testing", "019-https", "024-string-formatting", "025-regular-expressions", "026-sqlite", "027-structure-standards", "029-web-package", "030-readable-signatures"):
        codes = os.path.join(ROOT, "specs", spec, "contracts", "codes.md")
        for code, name, fix in re.findall(r"^\| ([EWRT]\d{4}) \| `(\w+)` \| [^|]* \| ([^|]*) \|",
                                          open(codes, encoding="utf-8").read(), re.M):
            out[code] = (name, "no" if code[0] in "RT" else fix.strip())
    return out


def golden_codes():
    found = set()
    for d, _, names in os.walk(os.path.join(ROOT, "tests", "cases")):
        for n in names:
            # step cases (tests/steps.py) keep their expected output in expected/out and files
            if n.endswith((".out", ".err")) or (os.path.basename(d) == "expected" or "/expected/" in d):
                found |= set(re.findall(r'"code":"([EWRT]\d{4})"', open(os.path.join(d, n), encoding="utf-8", errors="replace").read()))
                found |= set(re.findall(r"\[([EWRT]\d{4})\]", open(os.path.join(d, n), encoding="utf-8", errors="replace").read()))
    return found


def main():
    problems = []
    secs = sections()
    want = contract()
    have = {}
    for c, n, b in secs:
        m = re.search(r"^Fixable: (yes|sometimes|no)$", b, re.M)
        have[c] = (n, m.group(1) if m else "missing")
    for code, (name, fixable) in want.items():
        if code not in have:
            problems.append(f"{code}: missing from docs/errors.md")
        elif have[code] != (name, fixable):
            problems.append(f"{code}: catalog says {have[code]}, contract says {(name, fixable)}")
    for code in have:
        if code not in want:
            problems.append(f"{code}: in docs/errors.md but not in the contract")
    for code, name, body in secs:
        ex, fixed = part(body, "Example"), part(body, "Fixed")
        if "TODO" in ex or "TODO" in fixed or not ex.strip() or not fixed.strip():
            problems.append(f"{code}: Example or Fixed is not written")
            continue
        if code[0] == "T" or code in NO_SHORT_EXAMPLE:
            continue
        ex_blocks, fx_blocks = blocks(ex), blocks(fixed)
        if not ex_blocks or not fx_blocks:
            problems.append(f"{code}: Example and Fixed need ```vo blocks")
            continue
        with tempfile.TemporaryDirectory() as d:
            entry = write_program(ex_blocks, d)
            if code[0] == "R":
                limit = "2" if code == "R0009" else "30"
                _, recs = run(["run", "--json", "--timeout", limit], entry)
            else:
                _, recs = run(["check", "--json"], entry)
                if not any(r.get("code") == code for r in recs):
                    _, recs = run(["run", "--json", "--timeout", "2"], entry)
            if not any(r.get("code") == code for r in recs):
                got = sorted({r.get("code") for r in recs if r.get("code")})
                problems.append(f"{code}: the Example does not produce {code} (got {got})")
        with tempfile.TemporaryDirectory() as d:
            entry = write_program(fx_blocks, d)
            rc, recs = run(["check", "--json"], entry)
            # W0504 is advice about the project (create vo.effects), not about the example's code
            diags = [r["code"] for r in recs if r.get("kind") == "diagnostic"
                     and (r["code"] != "W0504" or code == "W0504")]
            if rc != 0 or diags:
                problems.append(f"{code}: the Fixed version does not check clean ({diags})")
            elif "fn main" in open(entry, encoding="utf-8").read():
                rc, recs = run(["run", "--json", "--timeout", "2"], entry)
                if rc != 0:
                    problems.append(f"{code}: the Fixed version exits {rc} ({[r.get('code') for r in recs]})")
    seen = golden_codes()
    for code in want:
        if code == "T0003":
            continue
        if code not in seen:
            problems.append(f"{code}: no golden case under tests/cases/ produces it")
    for p in problems:
        print("FAIL", p)
    print(f"catalog: {len(secs)} sections, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
