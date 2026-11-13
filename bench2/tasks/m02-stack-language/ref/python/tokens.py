LIMIT = 10 ** 18


def tokens(line):
    return [t for t in line.split(" ") if t]


def is_number(tok):
    d = tok[1:] if tok.startswith("-") else tok
    return d != "" and all(c in "0123456789" for c in d)


def number(tok):
    """Value of a number literal, or None when out of range."""
    v = int(tok)
    return v if -LIMIT <= v <= LIMIT else None
