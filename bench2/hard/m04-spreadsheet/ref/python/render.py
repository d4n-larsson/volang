"""Output: cell names, written values and the show grid."""
LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def name(cell):
    return LETTERS[cell[0]] + str(cell[1])


def written(v):
    if v is None:
        return ""
    return str(v[1])


def print_line(cell, v):
    return name(cell) + (" is empty" if v is None else " = " + written(v))


def formula_line(cell, c):
    if c is None:
        return name(cell) + " is empty"
    return name(cell) + " := " + str(c[1])


def deps_line(cell, cells):
    return name(cell) + " <- " + (" ".join(name(c) for c in cells) if cells else "none")


def grid(sheet, c1, r1, c2, r2):
    lw = len(str(r2))
    cols = list(range(c1, c2 + 1))
    vals = {(c, r): sheet.value((c, r)) for c in cols for r in range(r1, r2 + 1)}
    width = {c: max([1] + [len(written(vals[(c, r)])) for r in range(r1, r2 + 1)]) for c in cols}
    lines = [" " * lw + "".join(" | " + LETTERS[c].ljust(width[c]) for c in cols),
             "-" * lw + "".join("-+-" + "-" * width[c] for c in cols)]
    for r in range(r1, r2 + 1):
        parts = [str(r).rjust(lw)]
        for c in cols:
            v = vals[(c, r)]
            w = written(v)
            parts.append(" | " + (w.rjust(width[c]) if v is not None and v[0] == "n" else w.ljust(width[c])))
        lines.append("".join(parts))
    return [ln.rstrip(" ") for ln in lines]
