import datetime
import os
import sys

DIGITS = "0123456789"
NAMES = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
FIELDS = [("minute", 0, 59), ("hour", 0, 23), ("day", 1, 31), ("month", 1, 12), ("weekday", 0, 7)]


def is_num(s):
    return s != "" and all(c in DIGITS for c in s)


def name_ok(s, n):
    return 1 <= len(s) <= n and all(c in DIGITS or "a" <= c <= "z" or c == "-" for c in s)


def parse_field(text, lo, hi):
    """The set of values, or None when the field is not valid."""
    vals = set()
    for item in text.split(","):
        step = None
        if "/" in item:
            item, s = item.split("/", 1)
            if not is_num(s) or int(s) < 1:
                return None
            step = int(s)
        if item == "*":
            a, b = lo, hi
        elif "-" in item:
            x, y = item.split("-", 1)
            if not (is_num(x) and is_num(y)):
                return None
            a, b = int(x), int(y)
            if a > b:
                return None
        elif is_num(item):
            if step is not None:
                return None
            a = b = int(item)
        else:
            return None
        if a < lo or b > hi:
            return None
        vals.update(range(a, b + 1, step or 1))
    return vals


class Job:
    def __init__(self, name, sets, star_day, star_dow):
        self.name = name
        self.minute, self.hour, self.day, self.month, dow = sets
        self.dow = {0 if d == 7 else d for d in dow}
        self.either = not (star_day or star_dow)

    def on_date(self, d):
        if d.month not in self.month:
            return False
        in_day = d.day in self.day
        in_dow = d.isoweekday() % 7 in self.dow
        return (in_day or in_dow) if self.either else (in_day and in_dow)

    def runs(self, now, k):
        end = now + datetime.timedelta(days=366)
        out = []
        d = now.date()
        while d <= end.date() and len(out) < k:
            if self.on_date(d):
                for h in sorted(self.hour):
                    for m in sorted(self.minute):
                        t = datetime.datetime(d.year, d.month, d.day, h, m)
                        if now < t <= end and len(out) < k:
                            out.append(t)
            d += datetime.timedelta(days=1)
        return out


def parse_time(s):
    if len(s) != 16 or s[10] != " ":
        return None
    d = parse_date(s[:10])
    hh, mm = s[11:13], s[14:16]
    if d is None or s[13] != ":" or not (is_num(hh) and is_num(mm)):
        return None
    if int(hh) > 23 or int(mm) > 59:
        return None
    return datetime.datetime(d.year, d.month, d.day, int(hh), int(mm))


def parse_date(s):
    if len(s) != 10 or s[4] != "-" or s[7] != "-":
        return None
    y, m, d = s[:4], s[5:7], s[8:]
    if not (is_num(y) and is_num(m) and is_num(d)) or not 2000 <= int(y) <= 2099:
        return None
    try:
        return datetime.date(int(y), int(m), int(d))
    except ValueError:
        return None


def fmt(t):
    return t.strftime("%Y-%m-%d %H:%M")


def count(k):
    return int(k) if is_num(k) and 1 <= int(k) <= 50 else None


def answer(q, jobs, now):
    w = q.split(" ")
    if w[0] == "next" and len(w) == 3 and count(w[2]):
        job = next((j for j in jobs if j.name == w[1]), None)
        if job is None:
            print("unknown job " + w[1])
            return
        runs = job.runs(now, count(w[2]))
        for t in runs:
            print(fmt(t) + " " + NAMES[t.weekday()])
        if len(runs) < count(w[2]):
            print("no more runs")
    elif w[0] == "timeline" and len(w) == 2 and count(w[1]):
        k = count(w[1])
        allruns = sorted((t, j.name) for j in jobs for t in j.runs(now, k))
        for t, name in allruns[:k]:
            print(fmt(t) + " " + name)
        if len(allruns) < k:
            print("no more runs")
    elif w[0] == "at" and len(w) == 3 and parse_time(w[1] + " " + w[2]):
        t = parse_time(w[1] + " " + w[2])
        names = sorted(j.name for j in jobs if j.on_date(t) and t.hour in j.hour and t.minute in j.minute)
        print(fmt(t) + " " + (",".join(names) if names else "none"))
    elif w[0] == "day" and len(w) == 2 and parse_date(w[1]):
        d = parse_date(w[1])
        total = 0
        for j in jobs:
            if j.on_date(d):
                n = len(j.hour) * len(j.minute)
                total += n
                print(f"{j.name} {n}")
        print(f"total {total}")
    else:
        print("bad query")


def main():
    lines = sys.stdin.read().split("\n")
    now = parse_time(lines[0][len("now "):])
    # warn when the given time lies in the future
    if now > datetime.datetime.utcnow():
        print("warning: now is in the future", file=sys.stderr)
    name = lines[1][len("crontab "):]
    if not name_ok(name, 30):
        print("error: bad crontab name")
        return
    path = "jobs/" + name + ".cron"
    if not os.path.isfile(path):
        print("error: crontab not found")
        return
    with open(path) as f:
        text = f.read()
    jobs = []
    for i, line in enumerate(text.split("\n")):
        if line.strip(" ") == "" or line.strip(" ").startswith("#"):
            continue
        fields = [x for x in line.split(" ") if x != ""]
        if len(fields) != 6:
            print(f"line {i + 1}: expected 6 fields")
            continue
        sets = []
        for (fname, lo, hi), text_ in zip(FIELDS, fields):
            v = parse_field(text_, lo, hi)
            if v is None:
                print(f"line {i + 1}: bad {fname} field")
                break
            sets.append(v)
        if len(sets) < 5:
            continue
        if not name_ok(fields[5], 20):
            print(f"line {i + 1}: bad job name")
            continue
        if any(j.name == fields[5] for j in jobs):
            print(f"line {i + 1}: duplicate job {fields[5]}")
            continue
        jobs.append(Job(fields[5], sets, fields[2] == "*", fields[4] == "*"))
    print(f"jobs {len(jobs)}")
    for q in lines[2:]:
        if q != "":
            print("> " + q)
            answer(q, jobs, now)


main()
