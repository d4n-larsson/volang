"""Printing a select result as an aligned text table."""


def cell(v):
    return "NULL" if v is None else str(v)


def render(headers, types, rows):
    cells = [[cell(v) for v in r] for r in rows]
    widths = [max([len(h)] + [len(r[i]) for r in cells]) for i, h in enumerate(headers)]
    out = [" | ".join(h.ljust(w) for h, w in zip(headers, widths)),
           "-+-".join("-" * w for w in widths)]
    for r in cells:
        out.append(" | ".join(c.rjust(w) if ty == "int" else c.ljust(w) for c, w, ty in zip(r, widths, types)))
    out.append("(1 row)" if len(rows) == 1 else f"({len(rows)} rows)")
    return out
