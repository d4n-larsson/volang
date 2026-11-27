import sys
from datetime import date, timedelta

WEEKDAYS = ["mon", "tue", "wed", "thu", "fri", "sat", "sun"]
ORDS = {"1st": 1, "2nd": 2, "3rd": 3, "4th": 4, "5th": 5}


def parse_date(s):
    y, m, d = s.split("-")
    return date(int(y), int(m), int(d))


def days_in(y, m):
    if m == 12:
        return 31
    return (date(y, m + 1, 1) - date(y, m, 1)).days


def candidates(rule, start, last):
    """Dates the rule gives, in order, from start up to last (some may be before start)."""
    kind, n = rule[2], int(rule[1])
    if kind == "days":
        d = start
        while d <= last:
            yield d
            d += timedelta(days=n)
    elif kind == "weeks":
        wds = sorted({WEEKDAYS.index(w) for w in rule[4].split(",")})
        monday = start - timedelta(days=start.weekday())
        while monday <= last:
            for w in wds:
                yield monday + timedelta(days=w)
            monday += timedelta(weeks=n)
    elif kind == "months":
        idx = start.year * 12 + start.month - 1
        while True:
            y, m = divmod(idx, 12)
            m += 1
            if date(y, m, 1) > last:
                return
            dim = days_in(y, m)
            if rule[4] == "day":
                k = int(rule[5])
                day = k if k > 0 else dim + 1 + k
                if 1 <= day <= dim:
                    yield date(y, m, day)
            else:
                wd = WEEKDAYS.index(rule[5])
                first = (wd - date(y, m, 1).weekday()) % 7 + 1
                days = list(range(first, dim + 1, 7))
                if rule[4] == "last":
                    yield date(y, m, days[-1])
                elif ORDS[rule[4]] <= len(days):
                    yield date(y, m, days[ORDS[rule[4]] - 1])
            idx += n
    else:
        mm, dd = map(int, rule[4].split("-"))
        y = start.year
        while date(y, 1, 1) <= last:
            if dd <= days_in(y, mm):
                yield date(y, mm, dd)
            y += n


def main():
    lines = [l for l in sys.stdin.read().split("\n") if l]
    lo, hi = map(parse_date, lines[0].split())
    events, excepts = [], set()
    for line in lines[1:]:
        t = line.split(" ")
        if t[0] == "except":
            excepts.add((t[1], parse_date(t[2])))
        else:
            events.append((t[1], t[2:]))
    on = {}
    for idx, (name, rule) in enumerate(events):
        until, count = hi, None
        rest = rule
        if "count" in rest:
            count = int(rest[rest.index("count") + 1])
            rest = rest[:rest.index("count")]
        if "until" in rest:
            until = min(until, parse_date(rest[rest.index("until") + 1]))
            rest = rest[:rest.index("until")]
        start = parse_date(rest[rest.index("from") + 1])
        made = 0
        for d in candidates(rest, start, until):
            if d < start:
                continue
            if d > until or (count is not None and made == count):
                break
            made += 1
            if d >= lo and (name, d) not in excepts:
                on.setdefault(d, []).append(idx)
    out = []
    for d in sorted(on):
        names = [events[i][0] for i in sorted(set(on[d]))]
        out.append(f"{d.isoformat()} {['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'][d.weekday()]} {' '.join(names)}\n")
    sys.stdout.write("".join(out) or "no events\n")


main()
