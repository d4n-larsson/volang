import re
import sys
from functools import cmp_to_key

NUM = r"(0|[1-9][0-9]{0,17})"
IDENT = r"[0-9A-Za-z-]+"
VERSION = re.compile(rf"{NUM}\.{NUM}\.{NUM}(?:-({IDENT}(?:\.{IDENT})*))?(?:\+({IDENT}(?:\.{IDENT})*))?")


def parse(s):
    m = VERSION.fullmatch(s)
    if not m:
        return None
    pre = m.group(4).split(".") if m.group(4) is not None else []
    for p in pre:
        if p.isdigit() and ((len(p) > 1 and p[0] == "0") or len(p) > 18):
            return None
    return (int(m.group(1)), int(m.group(2)), int(m.group(3))), pre


def cmp_ident(a, b):
    an, bn = a.isdigit(), b.isdigit()
    if an and bn:
        return (int(a) > int(b)) - (int(a) < int(b))
    if an != bn:
        return -1 if an else 1
    return (a > b) - (a < b)


def cmp(x, y):
    if x[0] != y[0]:
        return -1 if x[0] < y[0] else 1
    if not x[1] or not y[1]:
        return (not x[1]) - (not y[1])
    for a, b in zip(x[1], y[1]):
        c = cmp_ident(a, b)
        if c:
            return c
    return (len(x[1]) > len(y[1])) - (len(x[1]) < len(y[1]))


def satisfies(x, comp):
    op, v = comp
    c = cmp(x, v)
    if op == "=":
        return c == 0
    if op == "<":
        return c < 0
    if op == "<=":
        return c <= 0
    if op == ">":
        return c > 0
    if op == ">=":
        return c >= 0
    (M, m, p), (xM, xm, xp) = v[0], x[0]
    if c < 0:
        return False
    if op == "~":
        return (xM, xm) == (M, m)
    if M != 0:
        return xM == M
    if m != 0:
        return (xM, xm) == (M, m)
    return (xM, xm, xp) == (M, m, p)


def main():
    data = sys.stdin.read()
    lines = data.split("\n")
    if data.endswith("\n"):
        lines.pop()
    sep = lines.index("%%")
    valid, invalid = [], []
    for line in lines[:sep]:
        if line:
            v = parse(line)
            (valid if v else invalid).append((line, v))
    ordered = sorted(valid, key=cmp_to_key(lambda a, b: cmp(a[1], b[1])))
    out = [l for l, _ in ordered] + ["invalid: " + l for l, _ in invalid]
    for q in lines[sep + 1:]:
        if not q:
            continue
        comps = []
        for t in q.split(" "):
            m = re.match(r"(<=|>=|<|>|=|\^|~)(.*)", t)
            comps.append((m.group(1), parse(m.group(2))))
        best = None
        for line, x in ordered:
            if all(satisfies(x, c) for c in comps) and (
                    not x[1] or any(v[1] and v[0] == x[0] for _, v in comps)):
                if best is None or cmp(x, best[1]) > 0:
                    best = (line, x)
        out.append(f"{q} -> {best[0] if best else 'none'}")
    sys.stdout.write("".join(l + "\n" for l in out))


main()
