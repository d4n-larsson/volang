#!/usr/bin/env python3
"""v1 -> v2 migration through fixes (US5, SC-005, FR-033).

Default: for every entry in tests/migrate/v1/MANIFEST (the frozen v1 corpus), copy it, check it,
require that only migration diagnostics appear (E0501, W0402 since spec 009, plus W0504 for multi-file
programs) besides
those v1 already reported, apply
the first fix of every diagnostic (up to three rounds, spec 056), then require a clean check, canonical formatting
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
MIGRATION = {"E0501", "W0504", "W0402"}  # spec 009: `case ok` / `case ok(_)` on a result[void], with a fix
# spec 018: advice about the corpus tests themselves (literal checks, tests that check nothing), not about
# the migration
MIGRATION |= {"W0602", "W0603"}
# spec 056: a library change (E0214), a local named like a builtin (E0203), each with a fix
MIGRATION |= {"E0214", "E0203", "E0105"}  # E0105: a number's spelling (`007`, `1E9`)
# Intended behavior changes since v1, with the spec that made them: (stdout, exit status) expected now.
CHANGED = {
    "run-oom_small/oom_small.vo": (b"58888890\n", 0),  # spec 003: memory is reclaimed, so it finishes
    # spec 042: upper and lower map Latin letters beyond ASCII ("w\u00f6rld" is now "W\u00d6RLD")
    "run-builtins_more/builtins_more.vo": (b'5 2.5\n3 7 1.5 apple\nHELLO, W\xc3\x96RLD abc\n65 233 B\xc3\xa9\n', 0),
    # spec 006 FR-025: a runtime error in a test lists its frames and their values
    "test-runtime_error/runtime_error.vo": (b"ERROR divides by zero (0ms)\n"
                                            b"  runtime_error.vo:2:12: runtime error[R0002]: Division by zero.\n"
                                            b"    at divide (runtime_error.vo:2:12)\n"
                                            b"        a: int = 1, b: int = 0\n"
                                            b"    at test \"divides by zero\" (runtime_error.vo:6:15)\n"
                                            b"PASS after the error (0ms)\n1 passed, 0 failed, 1 errored\n", 1),
    # spec 018: W0602/W0603 on the corpus tests, `expect` operands, the first difference of `expect_eq`
    'test-filter/filter.vo': (b'filter.vo:6:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n   6 |     expect(false);\n     |     ^^^^^^^^^^^^^\nfilter.vo:10:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n  10 |     expect(true);\n     |     ^^^^^^^^^^^^\nPASS parse numbers (0ms)\nFAIL format numbers (0ms)\n  filter.vo:6:5: `expect` failed: the condition is false.\nPASS parse words (0ms)\n2 passed, 1 failed\n', 1),
    'test-infinite/infinite.vo': (b'infinite.vo:1:1: warning[W0602]: `test \"loops forever\"` checks nothing: it has no `expect` or `expect_eq`, so it can only fail by a runtime error.\n   1 | test "loops forever" {\n     | ^^^^\ninfinite.vo:7:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n   7 |     expect(true);\n     |     ^^^^^^^^^^^^\nTIMEOUT loops forever (0ms)\n  infinite.vo:2:5: runtime error[R0009]: Time limit of 10 s exceeded.\nPASS still runs (0ms)\n1 passed, 0 failed, 1 timed out\n', 1),
    'test-values/values.vo': (b'values.vo:12:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n  12 |     expect_eq("abc", "abd");\n     |     ^^^^^^^^^^^^^^^^^^^^^^^\nvalues.vo:16:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n  16 |     expect_eq([1, 2], [1, 3]);\n     |     ^^^^^^^^^^^^^^^^^^^^^^^^^\nvalues.vo:20:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n  20 |     expect_eq(new Point { x: 1, y: 2 }, new Point { x: 1, y: 3 });\n     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\nvalues.vo:24:5: warning[W0603]: This check always has the same outcome: it does not depend on the program.\n  24 |     expect_eq(Shape.circle(1.0), Shape.empty);\n     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\nFAIL strings (0ms)\n  values.vo:12:5: `expect_eq` failed: the values differ.\n    expected: "abd"\n    actual:   "abc"\nFAIL lists (0ms)\n  values.vo:16:5: `expect_eq` failed: the values differ.\n    expected: [1, 3]\n    actual:   [1, 2]\n    first difference at [1]: expected 3, actual 2\nFAIL records (0ms)\n  values.vo:20:5: `expect_eq` failed: the values differ.\n    expected: new Point { x: 1, y: 3 }\n    actual:   new Point { x: 1, y: 2 }\n    first difference at .y: expected 3, actual 2\nFAIL variants (0ms)\n  values.vo:24:5: `expect_eq` failed: the values differ.\n    expected: Shape.empty\n    actual:   Shape.circle(1.0)\n0 passed, 4 failed\n', 1),
}

# Codes `vo check` reports now where v1 reported them only at run time (the run still stops the same way).
NOW_CHECKED = {"check-bad_main/bad_main.vo": {"E0409"}}  # inconsistency audit, 2026-11-25


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


def only_line_breaks(files, cwd):
    """Whether formatting changes nothing but line breaks and spaces (the 100-column wrapping that came
    after v1's canonical form)."""
    before = {f: open(f, encoding="utf-8").read() for f in files}
    with tempfile.TemporaryDirectory() as d:
        copies = []
        for f in files:
            c = os.path.join(d, os.path.relpath(f, cwd))
            os.makedirs(os.path.dirname(c), exist_ok=True)
            open(c, "w", encoding="utf-8").write(before[f])
            copies.append(c)
        vo(["fmt"] + copies, d)
        return all(re.sub(r"\s+", "", open(c, encoding="utf-8").read()) == re.sub(r"\s+", "", before[f])
                   for f, c in zip(files, copies))


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
            v1codes = set(open(base + ".v1diags").read().split()) | NOW_CHECKED.get(rel, set())
            other = sorted({d["code"] for d in diags} - MIGRATION - v1codes)
            if other:
                problems.append(f"{rel}: non-migration diagnostics {other}")
                continue
            for _ in range(3):  # spec 056: a fix can reveal the next one (a renamed item, then its new type)
                apply_first_fixes(entry, diags)
                diags = diagnostics(entry)
                after = [d for d in diags if d["severity"] == "error" and d["code"] not in NOW_CHECKED.get(rel, set())]
                if not after:
                    break
            if after:
                problems.append(f"{rel}: still {len(after)} error(s) after three rounds: "
                                f"{[d['code'] + ' ' + d['message'] for d in after][:3]}")
                continue
            files = [os.path.join(d, n) for d, _, ns in os.walk(os.path.dirname(entry)) for n in ns
                     if n.endswith(".vo")]
            v1canon = open(base + ".v1fmt").read().strip() == "canonical"
            if v1canon and vo(["fmt", "--check"] + files, os.path.dirname(entry)).returncode != 0 \
                    and not only_line_breaks(files, os.path.dirname(entry)):
                problems.append(f"{rel}: not canonical after the fixes (fmt --check)")
                continue
            mode = open(base + ".mode").read().strip()
            stdin = open(base + ".in", "rb").read() if os.path.exists(base + ".in") else b""
            r = vo([mode, os.path.basename(entry)], os.path.dirname(entry), stdin)
            want_out = open(base + ".v1out", "rb").read()
            want_code = int(open(base + ".v1code").read())
            want_out, want_code = CHANGED.get(rel, (want_out, want_code))
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
