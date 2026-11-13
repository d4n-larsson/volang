def digits(s, lo, hi):
    return lo <= len(s) <= hi and all(c in "0123456789" for c in s)


def cents(s):
    d, dot, c = s.partition(".")
    if digits(d, 1, 6) and dot and digits(c, 2, 2) and int(d + c) > 0:
        return int(d + c)
    return None


def parse(line):
    """(day, cmd, a, b) with b an int for numeric arguments; "blank"; or None if invalid."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    if len(f) != 4 or not digits(f[0], 1, 6):
        return None
    day, cmd, a, b = int(f[0]), f[1], f[2], f[3]
    if cmd == "book" and digits(b, 1, 2) and int(b) >= 1:
        return day, cmd, a, int(b)
    if cmd == "member" and len(b) == 1 and b in "123456789":
        return day, cmd, a, int(b)
    if cmd in ("borrow", "return"):
        return day, cmd, a, b
    if cmd == "pay" and cents(b) is not None:
        return day, cmd, a, cents(b)
    return None


def money(c):
    return f"{c // 100}.{c % 100:02d}"
