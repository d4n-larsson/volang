def count(s, k):
    if 1 <= len(s) <= k and all(c in "0123456789" for c in s):
        return int(s)
    return None


def quantity(s):
    q = count(s, 5)
    return q if q is not None and q >= 1 else None


def cost(s):
    d, dot, c = s.partition(".")
    if dot and count(d, 7) is not None and len(c) == 2 and count(c, 2) is not None:
        return int(d) * 100 + int(c)
    return None


def money(c):
    return f"{c // 100}.{c % 100:02d}"


# For each command: the converter of each argument (None keeps the field as it is).
FORMS = {
    "warehouse": [None],
    "item": [None],
    "receive": [None, None, quantity, cost, lambda s: count(s, 6)],
    "ship": [None, None, None, quantity],
    "reserve": [None, None, None, quantity],
    "fulfil": [None],
    "release": [None],
    "cancel": [None],
    "return": [None, quantity],
    "transfer": [None, None, None, quantity, lambda s: (lambda d: d if d is not None and d >= 1 else None)(count(s, 3))],
    "day": [lambda s: count(s, 6)],
    "stock": [],
    "value": [],
    "backorders": [],
}


def parse(line):
    """(command, args...) with values converted; "blank"; or None for an invalid line."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    form = FORMS.get(f[0])
    if form is None or len(f) - 1 != len(form):
        return None
    args = []
    for conv, s in zip(form, f[1:]):
        v = s if conv is None else conv(s)
        if v is None:
            return None
        args.append(v)
    return (f[0], *args)
