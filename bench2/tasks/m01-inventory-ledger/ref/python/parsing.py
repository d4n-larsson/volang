def is_number(f):
    return 1 <= len(f) <= 6 and all(c in "0123456789" for c in f)


def parse(line):
    """Returns a tuple (cmd, args...) or None when invalid; ("BLANK",) for a line without fields."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return ("BLANK",)
    if f[0] == "REPORT" and len(f) == 1:
        return ("REPORT",)
    if f[0] in ("RECEIVE", "SHIP") and len(f) == 4 and is_number(f[2]) and is_number(f[3]) and int(f[2]) > 0:
        return (f[0], f[1], int(f[2]), int(f[3]))
    if f[0] == "SCRAP" and len(f) == 3 and is_number(f[2]) and int(f[2]) > 0:
        return ("SCRAP", f[1], int(f[2]))
    return None
