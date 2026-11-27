"""Output formatting."""


def plural(n, word):
    if n == 1:
        return str(n) + " " + word
    return str(n) + " " + word + "s"


def numbered(buf, n):
    """Line n with its number, right-aligned to the width of the buffer's last line number."""
    width = len(str(buf.size()))
    return str(n).rjust(width) + "  " + buf.line(n)


def print_range(buf, a, b):
    out = [numbered(buf, n) for n in range(a, b + 1)]
    buf.cur = b
    return out


def goto(buf, n):
    buf.cur = n
    return [numbered(buf, n)]


def where(buf):
    return ["line " + str(buf.cur) + " of " + str(buf.size())]


def longest(buf):
    """The first of the longest lines."""
    best = 0
    for n in range(1, buf.size() + 1):
        if best == 0 or len(buf.line(n)) > len(buf.line(best)):
            best = n
    return ["longest line " + str(best) + " (" + plural(len(buf.line(best)), "byte") + ")"]
