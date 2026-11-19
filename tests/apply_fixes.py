#!/usr/bin/env python3
"""Applies every suggested fix in the golden outputs and re-checks (SC-003).

For each check/parse case whose expected JSONL has a diagnostic with fixes, the first fix is
applied to a copy of the case and `vo check --json` must no longer report that code at that
position (spec 024: also tests/cases/format). Also reports fix coverage over the error corpus
(tests/cases/{lex,parse,check,multi}).
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
CASES = os.path.join(ROOT, "tests", "cases")
# Stress cases are left out of the coverage measure: check/flood.vo is 60 distinct undefined
# names with nothing similar to suggest (it tests the 50-diagnostic cap, not realistic code).
STRESS = {"flood"}


def fixable_codes():
    text = open(os.path.join(ROOT, "docs", "errors.md"), encoding="utf-8").read()
    out = {}
    for m in re.finditer(r"^## ([EWRT]\d{4}) \w+\n(.*?)(?=^## |\Z)", text, re.M | re.S):
        f = re.search(r"^Fixable: (\w+)$", m.group(2), re.M)
        out[m.group(1)] = f.group(1) if f else "no"
    return out


def records(path):
    recs = []
    if not os.path.exists(path):
        return recs
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if line.startswith("{"):
            try:
                recs.append(json.loads(line))
            except ValueError:
                pass
    return recs


def offset(data, line, col):
    """Byte offset of a 1-based line/column (columns count code points)."""
    lines = data.split(b"\n")
    off = sum(len(l) + 1 for l in lines[:line - 1])
    text = lines[line - 1].decode("utf-8", "replace") if line - 1 < len(lines) else ""
    return off + len(text[:col - 1].encode("utf-8"))


def check_json(path):
    p = subprocess.run([VO, "check", "--json", path], capture_output=True, cwd=os.path.dirname(path))
    return [json.loads(l) for l in p.stdout.decode("utf-8", "replace").splitlines() if l.startswith("{")]


def json_cases(dirs):
    for d in dirs:
        base_dir = os.path.join(CASES, d)
        for dirpath, _, names in os.walk(base_dir):
            for n in sorted(names):
                if not n.endswith(".vo"):
                    continue
                base = os.path.join(dirpath, n[:-3])
                cmd = open(base + ".cmd").read().strip() if os.path.exists(base + ".cmd") else "check --json {}"
                if cmd.startswith("check --json") and os.path.exists(base + ".out"):
                    yield base, records(base + ".out")
                elif "--json" in cmd and os.path.exists(base + ".err"):
                    yield base, records(base + ".err")


def main():
    failures = []
    tried = resolved = 0
    for base, recs in json_cases(["check", "parse", "effects", "func", "generic", "format"]):
        for rec in recs:
            if rec.get("kind") != "diagnostic" or not rec.get("fixes"):
                continue
            tried += 1
            fix = rec["fixes"][0]
            case_dir = os.path.dirname(base)
            tmp = tempfile.mkdtemp(prefix="vofix-")
            try:
                for n in os.listdir(case_dir):
                    src = os.path.join(case_dir, n)
                    if os.path.isfile(src) and n.endswith(".vo"):
                        shutil.copy(src, tmp)
                    elif os.path.isdir(src):
                        shutil.copytree(src, os.path.join(tmp, n))
                target = os.path.join(tmp, fix["file"])
                data = open(target, "rb").read()
                s = offset(data, rec["start"]["line"], rec["start"]["col"])
                e = offset(data, rec["end"]["line"], rec["end"]["col"])
                diag_file = open(os.path.join(tmp, rec["file"]), "rb").read()
                ds = offset(diag_file, rec["start"]["line"], rec["start"]["col"])
                de = offset(diag_file, rec["end"]["line"], rec["end"]["col"])
                got = diag_file[ds:de].decode("utf-8", "replace")
                if got != rec["text"].replace("�", got if "�" in got else "�"):
                    failures.append(f"{base}: {rec['code']} text {rec['text']!r} != source {got!r}")
                    continue
                fs = offset(data, fix["start"]["line"], fix["start"]["col"])
                fe = offset(data, fix["end"]["line"], fix["end"]["col"])
                data = data[:fs] + fix["replacement"].encode("utf-8") + data[fe:]
                open(target, "wb").write(data)
                entry = os.path.join(tmp, os.path.basename(base) + ".vo")
                after = check_json(entry)
                again = [r for r in after if r.get("kind") == "diagnostic" and r["code"] == rec["code"]
                         and r["file"] == rec["file"] and r["start"] == rec["start"]]
                if again:
                    failures.append(f"{base}: applying the {rec['code']} fix did not remove it")
                else:
                    resolved += 1
            finally:
                shutil.rmtree(tmp, ignore_errors=True)
    fixable = fixable_codes()
    total = with_fix = fx_total = fx_with = st_total = st_with = 0
    for base, recs in json_cases(["lex", "parse", "check", "multi"]):
        if os.path.basename(base) in STRESS:
            continue
        for rec in recs:
            if rec.get("kind") != "diagnostic":
                continue
            total += 1
            has = bool(rec.get("fixes"))
            with_fix += has
            if fixable.get(rec["code"]) == "yes":
                fx_total += 1
                fx_with += has
            elif fixable.get(rec["code"]) == "sometimes":
                st_total += 1
                st_with += has
    for f in failures:
        print("FAIL", f)
    share = resolved / tried if tried else 1.0
    cover = fx_with / fx_total if fx_total else 1.0
    overall = with_fix / total if total else 1.0
    print(f"apply_fixes: {resolved}/{tried} fixes resolved their diagnostic ({share:.0%})")
    print(f"fix coverage: {fx_with}/{fx_total} diagnostics with fixable codes carry a fix ({cover:.0%}); "
          f"overall {with_fix}/{total} ({overall:.0%}); situational codes {st_with}/{st_total}")
    ok = not failures and share >= 0.95 and cover >= 0.80
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
