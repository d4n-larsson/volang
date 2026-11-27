import sys
from functools import lru_cache


def read_class(seg, i):
    """seg[i] is '['. Returns (token, next index) or None if the class is not closed."""
    j = i + 1
    neg = j < len(seg) and seg[j] == "!"
    if neg:
        j += 1
    members = []          # (lo, hi) code point ranges
    first = True

    def member(k):
        """The member at k: (char, next) or None."""
        if k >= len(seg):
            return None
        if seg[k] == "\\":
            return (seg[k + 1], k + 2) if k + 1 < len(seg) else None
        if seg[k] == "]" and not (first and k == j0):
            return None
        return seg[k], k + 1

    j0 = j
    while True:
        m = member(j)
        if m is None:
            if j < len(seg) and seg[j] == "]":
                return ("class", neg, members), j + 1
            return None
        first = False
        x, j = m
        if j < len(seg) and seg[j] == "-":
            y = member(j + 1)
            if y is not None:
                members.append((ord(x), ord(y[0])))
                j = y[1]
                continue
        members.append((ord(x), ord(x)))


def tokens(seg):
    out, i = [], 0
    while i < len(seg):
        c = seg[i]
        if c == "?":
            out.append(("any",))
            i += 1
        elif c == "*":
            out.append(("star",))
            i += 1
        elif c == "\\":
            out.append(("lit", seg[i + 1] if i + 1 < len(seg) else "\\"))
            i += 2
        elif c == "[":
            r = read_class(seg, i)
            if r is None:
                out.append(("lit", "["))
                i += 1
            else:
                out.append(r[0])
                i = r[1]
        else:
            out.append(("lit", c))
            i += 1
    return out


def one(tok, c):
    if tok[0] == "any":
        return True
    if tok[0] == "lit":
        return tok[1] == c
    _, neg, members = tok
    inside = any(lo <= ord(c) <= hi for lo, hi in members)
    return inside != neg


def seg_match(toks, s):
    if s.startswith(".") and not (toks and toks[0] == ("lit", ".")):
        return False
    # reach[j]: tokens so far can match s[:j]
    reach = [True] + [False] * len(s)
    for t in toks:
        if t[0] == "star":
            for j in range(1, len(s) + 1):
                reach[j] = reach[j] or reach[j - 1]
        else:
            reach = [False] + [reach[j] and one(t, s[j]) for j in range(len(s))]
    return reach[len(s)]


def matches(pat, path):
    psegs = [None if p == "**" else tokens(p) for p in pat.split("/")]
    segs = path.split("/")

    @lru_cache(maxsize=None)
    def go(i, j):
        if i == len(psegs):
            return j == len(segs)
        if psegs[i] is None:
            if go(i + 1, j):
                return True
            return j < len(segs) and not segs[j].startswith(".") and go(i, j + 1)
        return j < len(segs) and seg_match(psegs[i], segs[j]) and go(i + 1, j + 1)

    return go(0, 0)


def main():
    data = sys.stdin.buffer.read().decode("utf-8")
    lines = data.split("\n")
    if data.endswith("\n"):
        lines.pop()
    sep = lines.index("%%")
    pats = lines[:sep]
    out = []
    for path in lines[sep + 1:]:
        hit = [str(k + 1) for k, p in enumerate(pats) if matches(p, path)]
        out.append(f"{path} -> {','.join(hit) or 'none'}\n")
    sys.stdout.write("".join(out))


main()
