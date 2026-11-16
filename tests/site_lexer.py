#!/usr/bin/env python3
"""Spec 015 (T006): the site's vo lexer highlights every vo block of the reference and the site's pages
with no error tokens. Runs with the site's virtual environment (make site-check)."""
import glob
import os
import re
import sys

from pygments import lex
from pygments.lexers import get_lexer_by_name
from pygments.token import Error

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
lexer = get_lexer_by_name("vo")
bad = blocks = 0
for path in [os.path.join(ROOT, "docs", "reference.md")] + sorted(glob.glob(os.path.join(ROOT, "site", "docs", "**", "*.md"), recursive=True)):
    for lang, code in re.findall(r"^```(vo|vo-test|vo-fmt)\n(.*?)^```\n", open(path, encoding="utf-8").read(), re.M | re.S):
        blocks += 1
        errs = [v for t, v in lex(code, lexer) if t is Error]
        if errs:
            bad += 1
            print(f"FAIL {os.path.relpath(path, ROOT)}: error tokens {errs[:5]!r}")
print(f"site_lexer: {blocks} blocks, {bad} with error tokens")
sys.exit(1 if bad else 0)
