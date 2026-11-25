#!/usr/bin/env python3
"""Spec 056 (FR-022): the diagnostic messages in src/*.c follow one style (docs/errors.md's style note).

Each C string literal that is (part of) a message is checked for:
  - "got" where the style says "found";
  - a range written `A-B` with numbers (the style writes "A to B");
  - a test named another way than `test "NAME"` (`Test `NAME``, `test named ...`), a package
    named without backticks (`package $1`);
  - code left without backticks: `\\n`, `@size(...)`-style annotations, `0x` literals.
Lines with `style: ok` in a comment are skipped (a quoted protocol text, say)."""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
RULES = [
    ("got", re.compile(r"\bgot\b")),
    ("range with -", re.compile(r"(?<![\w`.$-])\d+-\d+(?![\w`])")),
    ("test name", re.compile(r"\bTest `|\btest named\b")),
    ("package name", re.compile(r"(?<!`vo )\b[Pp]ackage \$\d")),
    ("code without backticks", re.compile(r"(?<!`)(?:@(?:size|range|pattern|default|header|allow|spec)\(|\b0x[0-9A-Fa-f]+\b)(?![^`]*`)")),
]


def messages(text):
    """(line number, literal) for string literals on lines that build a diagnostic or a message."""
    for n, line in enumerate(text.split("\n"), 1):
        if "style: ok" in line or line.lstrip().startswith(("#", "//", "/*", "*")):
            continue
        for m in LIT.finditer(line):
            s = m.group(1)
            if len(s) > 12 and " " in s and s[:1].isupper() or re.match(r"[`$]", s[:1] or "x") and " " in s:
                yield n, s


def main():
    problems = []
    for f in sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))):
        if os.path.basename(f).startswith(("sqlite3", "mbedtls", "core_math")):
            continue
        for n, s in messages(open(f, encoding="utf-8", errors="replace").read()):
            for name, rx in RULES:
                if rx.search(s):
                    problems.append(f"{os.path.relpath(f, ROOT)}:{n}: {name}: {s[:110]}")
    for p in problems:
        print("FAIL " + p)
    print(f"message_style: {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
