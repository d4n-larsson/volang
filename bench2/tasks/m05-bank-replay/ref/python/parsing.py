DIGITS = "0123456789"


def digits(s, lo, hi):
    return lo <= len(s) <= hi and all(c in DIGITS for c in s)


def money(s):
    """Cents, or None."""
    d, dot, c = s.partition(".")
    if not digits(d, 1, 9) or (dot and not digits(c, 1, 2)):
        return None
    return int(d) * 100 + (int(c.ljust(2, "0")) if dot else 0)


FORMS = {"open": ["name", "money"], "deposit": ["name", "amount"], "withdraw": ["name", "amount"],
         "transfer": ["name", "name", "amount"], "month": [], "close": ["name"]}


def parse(line):
    """(time, command, args) or None when invalid; "blank" for a line without fields."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    if len(f) < 2 or not digits(f[0], 1, 9) or f[1] not in FORMS:
        return None
    form = FORMS[f[1]]
    if len(f) != 2 + len(form):
        return None
    args = []
    for kind, v in zip(form, f[2:]):
        if kind == "name":
            args.append(v)
        else:
            m = money(v)
            if m is None or (kind == "amount" and m == 0):
                return None
            args.append(m)
    return int(f[0]), f[1], args


def show(c):
    sign = "-" if c < 0 else ""
    c = abs(c)
    return f"{sign}{c // 100}.{c % 100:02d}"
