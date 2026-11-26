#!/usr/bin/env python3
"""Spec 059 (SC-001, SC-003): counts anonymous functions in vo files and Markdown code blocks.

    python3 tests/lambda_stats.py [PATH...]      (default: examples std)

An anonymous function is `fn(` after `(`, `,`, `=`, `[`, `return` or `{` (a function *type* follows `:`
or `->`), whose header ends at `{` or `=>`. Prints, per path: lambdas, those with a written parameter or
return type, characters of their headers (from `fn` to the body), single-return lambdas
(`{ return e; }` or `=> e`) and the lines they span. Comment lines count (std's doc tests are code).
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BEFORE = re.compile(r"(?:[(,=\[{]|\breturn)\s*$")


def close(text, i, open_, shut):
    """Index just after the bracket matching text[i] (== open_), or -1."""
    depth = 0
    while i < len(text):
        c = text[i]
        if c == '"':
            i = text.find('"', i + 1)
            while i > 0 and text[i - 1] == "\\":
                i = text.find('"', i + 1)
            if i < 0:
                return -1
        elif c == open_:
            depth += 1
        elif c == shut:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def lambdas(text):
    for m in re.finditer(r"\bfn\(", text):
        if not BEFORE.search(text[max(0, m.start() - 12):m.start()]):
            continue
        params_end = close(text, m.end() - 1, "(", ")")
        if params_end < 0:
            continue
        brace, arrow = text.find("{", params_end), text.find("=>", params_end)
        body = min(x for x in (brace, arrow) if x >= 0) if max(brace, arrow) >= 0 else -1
        if body < 0 or "\n" in text[params_end:body] and "uses" not in text[params_end:body]:
            continue
        if re.search(r"[;,)]", re.sub(r"\[[^\[\]]*\]|\([^()]*\)", "", text[params_end:body])):
            continue  # a function *type* (it goes on with `,`, `)` or `;`), not a lambda
        header = text[m.start():body].rstrip()
        params = text[m.end():params_end - 1]
        typed = ":" in params or "->" in text[params_end:body]
        if body == arrow:
            ends = [x for x in (text.find(";", body), text.find("\n", body)) if x >= 0]
            single, stop = True, min(ends) if ends else len(text)  # an arrow body: to its line's end at most
        else:
            stop = close(text, body, "{", "}")
            inner = text[body + 1:stop - 1].strip()
            single = bool(re.fullmatch(r"return\b[^;]*;", inner, re.S))
        yield typed, len(header), single, text.count("\n", m.start(), stop) + 1


def files(path):
    if os.path.isfile(path):
        yield path
        return
    for d, _, names in sorted(os.walk(path)):
        for n in sorted(names):
            if n.endswith((".vo", ".md")):
                yield os.path.join(d, n)


def source(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    if path.endswith(".md"):  # only code blocks
        text = "\n".join(re.findall(r"```(?:vo|vo-test)\n(.*?)```", text, re.S))
    return text


def main():
    paths = sys.argv[1:] or ["examples", "std"]
    total = [0] * 6
    for p in paths:
        row = [0] * 6  # lambdas, typed, header chars, single, single lines, lines
        for f in files(os.path.join(ROOT, p) if not os.path.isabs(p) else p):
            for typed, chars, single, lines in lambdas(source(f)):
                row[0] += 1
                row[1] += typed
                row[2] += chars
                row[3] += single
                row[4] += lines if single else 0
                row[5] += lines
        total = [a + b for a, b in zip(total, row)]
        print(f"{p}: {row[0]} lambdas, {row[1]} with written types, {row[2]} header characters, "
              f"{row[3]} single-return spanning {row[4]} lines (all lambdas: {row[5]} lines)")
    if len(paths) > 1:
        print(f"total: {total[0]} lambdas, {total[1]} with written types, {total[2]} header characters, "
              f"{total[3]} single-return spanning {total[4]} lines (all lambdas: {total[5]} lines)")


if __name__ == "__main__":
    main()
