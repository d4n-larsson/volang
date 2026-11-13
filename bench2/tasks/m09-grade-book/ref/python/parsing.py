def digits(s, lo, hi):
    return lo <= len(s) <= hi and all(c in "0123456789" for c in s)


def parse(line):
    """A tuple of the fields with numbers converted, "blank", or None if the form is wrong."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    if f[0] == "assignment" and len(f) == 4 and digits(f[2], 1, 3) and digits(f[3], 1, 4) \
            and int(f[2]) >= 1 and int(f[3]) >= 1:
        return ("assignment", f[1], int(f[2]), int(f[3]))
    if f[0] == "student" and len(f) == 2:
        return ("student", f[1])
    if f[0] == "score" and len(f) == 4 and digits(f[3], 1, 4):
        return ("score", f[1], f[2], int(f[3]))
    if f[0] == "drop" and len(f) == 2 and digits(f[1], 1, 1):
        return ("drop", int(f[1]))
    return None


def pct(bp):
    return f"{bp // 100}.{bp % 100:02d}"
