DAYS = ("Mon", "Tue", "Wed", "Thu", "Fri")


def digits(s):
    return s != "" and all(c in "0123456789" for c in s)


def time(s):
    if len(s) != 5 or s[2] != ":" or not digits(s[:2]) or not digits(s[3:]):
        return None
    h, m = int(s[:2]), int(s[3:])
    return h * 60 + m if h < 24 and m < 60 else None


def people(s):
    ps = s.split(",")
    if "" in ps or len(set(ps)) != len(ps):
        return None
    return ps


def parse(line):
    """("meeting", id, room, day, start, end, people) / ("free", day, minutes, people) /
    "blank" / None (invalid)."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    if f[0] == "meeting" and len(f) == 6 and f[3] in DAYS:
        span = f[4].split("-")
        if len(span) != 2:
            return None
        s, e, ps = time(span[0]), time(span[1]), people(f[5])
        if s is None or e is None or e <= s or ps is None:
            return None
        return ("meeting", f[1], f[2], f[3], s, e, ps)
    if f[0] == "free" and len(f) == 4 and f[1] in DAYS and digits(f[2]) and len(f[2]) <= 3 \
            and int(f[2]) > 0 and people(f[3]) is not None:
        return ("free", f[1], int(f[2]), people(f[3]))
    return None


def clock(m):
    return f"{m // 60:02d}:{m % 60:02d}"
