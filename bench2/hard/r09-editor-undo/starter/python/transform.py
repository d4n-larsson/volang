"""Commands that change the text of lines in place."""
import view


def subst(buf, a, b, old, new):
    total = 0
    for n in range(a, b + 1):
        k = buf.line(n).count(old)
        if k > 0:
            buf.lines[n - 1] = buf.line(n).replace(old, new)
            total += k
    if total > 0:
        buf.cur = b
    return [view.plural(total, "substitution")]


def map_lines(buf, a, b, f):
    buf.put_block(a, b, [f(line) for line in buf.block(a, b)])
    buf.cur = b
    return []


def upper_lines(buf, a, b):
    return map_lines(buf, a, b, lambda s: s.upper())


def lower_lines(buf, a, b):
    return map_lines(buf, a, b, lambda s: s.lower())


def strip(buf, a, b):
    return map_lines(buf, a, b, lambda s: s.strip(" "))


def indent(buf, a, b, n):
    return map_lines(buf, a, b, lambda s: " " * n + s)


def sort_lines(buf, a, b):
    buf.put_block(a, b, sorted(buf.block(a, b)))
    buf.cur = b
    return []


def reverse(buf, a, b):
    buf.put_block(a, b, list(reversed(buf.block(a, b))))
    buf.cur = b
    return []


def dedupe(buf):
    """Removes every line that equals the line right before it."""
    kept = []
    for line in buf.lines:
        if len(kept) == 0 or kept[-1] != line:
            kept.append(line)
    removed = buf.size() - len(kept)
    buf.lines = kept
    buf.clamp_cur()
    return ["removed " + view.plural(removed, "line")]


def wrap_one(text, width):
    """The words of text filled greedily into lines of at most width bytes."""
    out = []
    current = ""
    for w in text.split(" "):
        if w == "":
            continue
        if current == "":
            current = w
        elif len(current) + 1 + len(w) <= width:
            current = current + " " + w
        else:
            out.append(current)
            current = w
    out.append(current)
    return out


def wrap(buf, a, b, width):
    new_lines = []
    for line in buf.block(a, b):
        new_lines.extend(wrap_one(line, width))
    buf.put_block(a, b, new_lines)
    buf.cur = a + len(new_lines) - 1
    return []
