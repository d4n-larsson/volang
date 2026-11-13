def is_tag(t):
    return t != "" and all(c.isascii() and (c.islower() or c.isdigit()) for c in t)


def is_prio(p):
    return len(p) == 1 and p in "12345"


def parse(line):
    """Returns (cmd, fields) or None when the line has no valid form; ("", []) for a blank line."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return ("", [])
    c, n = f[0], len(f)
    if c == "add" and n >= 3 and is_prio(f[1]):
        return (c, [f[1], " ".join(f[2:])])
    if c in ("done", "reopen", "delete") and n == 2:
        return (c, f[1:])
    if c == "tag" and n == 3 and is_tag(f[2]):
        return (c, f[1:])
    if c == "prio" and n == 3 and is_prio(f[2]):
        return (c, f[1:])
    if c == "list" and n == 1:
        return (c, [])
    if c == "list" and n == 2 and (f[1] == "done" or (f[1][0] == "+" and is_tag(f[1][1:]))):
        return (c, f[1:])
    if c == "stats" and n == 1:
        return (c, [])
    return None
