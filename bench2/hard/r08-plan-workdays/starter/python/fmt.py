# Text helpers shared by the reports.


def pad_right(s, width):
    return s + " " * (width - len(s))


def pad_left(s, width):
    return " " * (width - len(s)) + s


def table(header, rows):
    """Columns separated by two spaces; the first column left-aligned, the others right-aligned."""
    widths = [len(h) for h in header]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))
    out = []
    for row in [header] + rows:
        cells = [pad_right(row[0], widths[0])]
        for i in range(1, len(row)):
            cells.append(pad_left(row[i], widths[i]))
        out.append("  ".join(cells))
    return out


def dep_text(dep):
    if dep.lag > 0:
        return dep.pred + "+" + str(dep.lag)
    return dep.pred


def id_list(ids):
    if len(ids) == 0:
        return "-"
    return ", ".join(ids)


def percent(part, whole):
    """part * 100 / whole rounded down, as text; `-` when whole is 0."""
    if whole == 0:
        return "-"
    return str(part * 100 // whole) + "%"
