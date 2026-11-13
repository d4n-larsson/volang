import sys


def parse(t):
    sign = 1
    if t[0] in "+-":
        sign = -1 if t[0] == "-" else 1
        t = t[1:]
    if not t or not all("0" <= c <= "9" for c in t):
        return None
    t = t.lstrip("0") or "0"
    return (1 if t == "0" else sign), t


def less(a, b):
    return (len(a), a) < (len(b), b)


def add(a, b):
    out, carry = [], 0
    a, b = a[::-1], b[::-1]
    for i in range(max(len(a), len(b))):
        d = carry + (int(a[i]) if i < len(a) else 0) + (int(b[i]) if i < len(b) else 0)
        out.append(str(d % 10))
        carry = d // 10
    if carry:
        out.append("1")
    return "".join(out[::-1])


def sub(a, b):
    """a - b for a >= b."""
    out, borrow = [], 0
    a, b = a[::-1], b[::-1]
    for i in range(len(a)):
        d = int(a[i]) - borrow - (int(b[i]) if i < len(b) else 0)
        borrow = 1 if d < 0 else 0
        out.append(str(d + 10 * borrow))
    return "".join(out[::-1]).lstrip("0") or "0"


def answer(line):
    t = line.split(" ")
    t = [x for x in t if x]
    if len(t) != 2:
        return "invalid"
    x, y = parse(t[0]), parse(t[1])
    if x is None or y is None:
        return "invalid"
    (sa, a), (sb, b) = x, y
    if sa == sb:
        r = add(a, b)
        return r if sa > 0 or r == "0" else "-" + r
    if less(a, b):
        sa, a, sb, b = sb, b, sa, a
    r = sub(a, b)
    return "-" + r if sa < 0 and r != "0" else r


text = sys.stdin.read()
lines = text.split("\n")
if text.endswith("\n"):
    lines.pop()
for line in lines if text else []:
    print(answer(line))
