LEVEL = {"+": 1, "-": 1, "*": 2, "/": 2, "%": 2, "^": 4}


def level(e):
    if e[0] == "bin":
        return LEVEL[e[1]]
    return 3 if e[0] == "neg" else 5


def wrap(e, needed):
    s = show(e)
    return f"({s})" if needed else s


def show(e):
    if e[0] == "num":
        return e[1].lstrip("0") or "0"
    if e[0] == "name":
        return e[1]
    if e[0] == "neg":
        return "-" + wrap(e[1], level(e[1]) < 3)
    op, l, r = e[1], e[2], e[3]
    if op == "^":
        return wrap(l, level(l) <= 4) + " ^ " + wrap(r, level(r) < 3)
    p = LEVEL[op]
    return wrap(l, level(l) < p) + f" {op} " + wrap(r, level(r) <= p)
