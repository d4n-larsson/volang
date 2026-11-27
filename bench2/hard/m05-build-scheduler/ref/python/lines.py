"""Reading the input lines: syntax, settings, duplicates and unknown names."""

LIMITS = {"workers": (1, 9), "retries": (0, 9), "backoff": (0, 1000), "timeout": (0, 1000)}


def is_name(s):
    return len(s) > 0 and all(c == "_" or "a" <= c <= "z" or "0" <= c <= "9" for c in s)


def number(s, lo, hi):
    """The value of s if it is a number within lo..hi, else None."""
    if not (1 <= len(s) <= 4) or not all("0" <= c <= "9" for c in s):
        return None
    v = int(s)
    return v if lo <= v <= hi else None


class Build:
    def __init__(self):
        self.settings = {"workers": 1, "retries": 0, "backoff": 0, "timeout": 0}
        self.targets = {}        # name -> (duration, [deps], line number), first definition only
        self.order = []          # target names in definition order
        self.fails = set()       # (name, attempt)
        self.pools = {}          # pool name -> size, first definition only
        self.pool_of = {}        # target name -> pool name
        self.goals = []          # defined goal names (in order, may repeat)
        self.has_goal_line = False
        self.problems = []       # (line, text)


def parse_line(fields):
    """A well-formed line as (kind, payload), or None if malformed."""
    kind = fields[0]
    if kind == "target":
        if len(fields) < 3 or not all(is_name(f) for f in [fields[1]] + fields[3:]):
            return None
        d = number(fields[2], 1, 1000)
        return None if d is None else ("target", (fields[1], d, fields[3:]))
    if kind in LIMITS:
        if len(fields) != 2:
            return None
        v = number(fields[1], *LIMITS[kind])
        return None if v is None else (kind, v)
    if kind == "fail":
        if len(fields) != 3 or not is_name(fields[1]):
            return None
        a = number(fields[2], 1, 10)
        return None if a is None else ("fail", (fields[1], a))
    if kind == "pool" or kind == "use":
        if len(fields) != 3 or not is_name(fields[1]):
            return None
        if kind == "use":
            return ("use", (fields[1], fields[2])) if is_name(fields[2]) else None
        n = number(fields[2], 1, 9)
        return None if n is None else ("pool", (fields[1], n))
    if kind == "goal":
        if len(fields) < 2 or not all(is_name(f) for f in fields[1:]):
            return None
        return ("goal", fields[1:])
    return None


def first_seen(names):
    out = []
    for n in names:
        if n not in out:
            out.append(n)
    return out


def read(text):
    b = Build()
    parsed = []
    for i, line in enumerate(text.split("\n"), 1):
        fields = [f for f in line.split(" ") if f]
        if not fields:
            continue
        p = parse_line(fields)
        if p is None:
            b.problems.append((i, "malformed"))
        else:
            parsed.append((i, p))
    defining = set()
    for i, (kind, val) in parsed:
        if kind == "target":
            name, d, deps = val
            if name in b.targets:
                b.problems.append((i, f"duplicate target {name}"))
            else:
                b.targets[name] = (d, first_seen(deps), i)
                b.order.append(name)
                defining.add(i)
        elif kind == "pool":
            name, n = val
            if name in b.pools:
                b.problems.append((i, f"duplicate pool {name}"))
            else:
                b.pools[name] = n
    for i, (kind, val) in parsed:
        if kind in LIMITS:
            b.settings[kind] = val
        elif kind == "target" and i in defining:
            name, _, deps = val
            for dep in first_seen(deps):
                if dep not in b.targets:
                    b.problems.append((i, f"unknown dependency {dep} of {name}"))
        elif kind == "fail":
            name, a = val
            if name in b.targets:
                b.fails.add((name, a))
            else:
                b.problems.append((i, f"unknown target {name}"))
        elif kind == "use":
            name, pool = val
            if name not in b.targets:
                b.problems.append((i, f"unknown target {name}"))
            if pool not in b.pools:
                b.problems.append((i, f"unknown pool {pool}"))
            if name in b.targets and pool in b.pools:
                b.pool_of[name] = pool
        elif kind == "goal":
            b.has_goal_line = True
            for n in first_seen(val):
                if n in b.targets:
                    b.goals.append(n)
                else:
                    b.problems.append((i, f"unknown target {n}"))
    b.problems.sort(key=lambda p: p[0])
    return b
