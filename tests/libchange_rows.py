#!/usr/bin/env python3
"""Spec 056 (SC-003): every row of the library change table (src/libchange.c) reports E0214 with its fix.
For each row, a minimal program using the old form is checked: it must report E0214, and after the first
fix of every diagnostic (tools/vo_fix.vo) it must check clean (spec 057: rows that added an argument too). Rows of a kind without a template here are
listed as skipped."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
sys.path.insert(0, os.path.join(ROOT, "tests"))
from apply_fixes import diagnostics, vo_fix  # noqa: E402

_S = r'(?:"((?:[^"\\]|\\.)*)"|0)'  # a C string (escapes allowed) or 0
ROW = re.compile(r'\{"(\w+)",\s*"([\w.]+)",\s*(LC_\w+),\s*' + _S + r',\s*' + _S + r',\s*' + _S + r',\s*(-?\d+)\}')


def rows():
    src = open(os.path.join(ROOT, "src", "libchange.c")).read()
    return [m.groups() for m in ROW.finditer(src)]


def program(row):
    """The source of a program using the old form of the row, or None (no template for its kind)."""
    module, old, kind, new, call, value, factor = row
    if kind in ("LC_RENAME", "LC_REMOVE") and "." not in old:
        return f"import std.{module}.{old};\n\nfn main() {{\n}}\n"
    if kind == "LC_OPTION" and old in LOOKUP:
        imports, param, args, t = LOOKUP[old]
        lines = [f"import std.{module}.{old};"] + [f"import {i};" for i in imports]
        return "\n".join(lines) + f"\n\nfn f({param}) -> result[{t}] {{\n    return {old}({args});\n}}\n\nfn main() {{\n}}\n"
    if kind == "LC_SWAP":
        extra = ', "y"' if old == "replace_matches" else ""
        return (f"import std.{module}.{old};\nimport std.regex.regex;\n\n"
                f"fn main() {{\n    let _ = {old}(regex(\"a\"), \"x\"{extra});\n}}\n")
    if kind == "LC_FIELD":
        record, field = old.split(".")
        tp, ta = ("[C, E: effects]", "[C, E]") if record == "Browser" else ("", "")
        return (f"import std.{module}.{record};\n\n"
                f"fn f{tp}(x: {record}{ta}) {{\n    let _ = x.{field};\n}}\n\nfn main() {{\n}}\n")
    if kind == "LC_ARG":  # spec 057: the 0.2 call, one argument fewer
        args = {"run_with": '"true", [], "", []', "run_with_bytes": '"true", [], from_text(""), []',
                "run_lines": '"true", [], "", [], fn(line: str) -> result[bool] { return ok(true); }'}[old]
        imp = "import std.bytes.from_text;\n" if old == "run_with_bytes" else ""
        return (f"{imp}import std.{module}.{old};\n\n"
                f"fn main() -> result[void] uses process(\"true\") {{\n    let _ = try {old}({args});\n"
                f"    return ok();\n}}\n")
    if kind == "LC_CASE":
        variant, case = old.split(".")
        return (f"import std.{module}.{variant};\n\n"
                f"fn f(e: {variant}) -> bool {{\n    return e == {variant}.{case};\n}}\n\nfn main() {{\n}}\n")
    return None


# A lookup row: (imports, the parameter, the call's arguments, the element type).
_M, _S = ("m: ordered_map[int, int]", "int"), ("s: ordered_set[int]", "int")
LOOKUP = {
    "first_key": ((), _M[0], "m", "int"), "last_key": ((), _M[0], "m", "int"),
    "key_at_or_above": ((), _M[0], "m, 1", "int"), "key_at_or_below": ((), _M[0], "m, 1", "int"),
    "key_after": ((), _M[0], "m, 1", "int"), "key_before": ((), _M[0], "m, 1", "int"),
    "first": ((), _S[0], "s", "int"), "last": ((), _S[0], "s", "int"),
    "at_or_above": ((), _S[0], "s, 1", "int"), "at_or_below": ((), _S[0], "s, 1", "int"),
    "after": ((), _S[0], "s, 1", "int"), "before": ((), _S[0], "s, 1", "int"),
    "peek": ((), "q: priority_queue[int]", "q", "int"),
    "front": ((), "d: deque[int]", "d", "int"), "back": ((), "d: deque[int]", "d", "int"),
    "request_header": (("std.http.Request",), "r: Request", 'r, "Host"', "str"),
    "response_header": (("std.http.Response",), "r: Response", 'r, "Host"', "str"),
    "first_match": (("std.regex.Regex", "std.regex.Match"), "re: Regex", '"x", re', "Match"),
}


def main():
    problems, skipped, checked = [], [], 0
    for row in rows():
        src = program(row)
        label = f"std.{row[0]}.{row[1]} ({row[2][3:].lower()})"
        if src is None:
            skipped.append(label)
            continue
        d = tempfile.mkdtemp(prefix="vo-lc-")
        try:
            path = os.path.join(d, "main.vo")
            open(path, "w").write(src)
            codes = {r["code"] for r in diagnostics(path)}
            if "E0214" not in codes:
                problems.append(f"{label}: no E0214 (got {sorted(codes)})")
                continue
            fixed, left = vo_fix([path])
            if left:
                problems.append(f"{label}: not clean after the fix: {left[:2]}")
            checked += 1
        finally:
            shutil.rmtree(d, ignore_errors=True)
    for p in problems:
        print("FAIL", p)
    if skipped:
        print("skipped (no template for the kind):", ", ".join(skipped))
    print(f"libchange_rows: {checked} rows checked, {len(skipped)} skipped, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
