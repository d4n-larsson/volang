import datetime
import os
import sys

RULES = ["last", "daily", "weekly", "monthly", "yearly", "within"]
NAME_CHARS = set("abcdefghijklmnopqrstuvwxyz0123456789-")
DIGITS = set("0123456789")


def is_digits(s):
    return s != "" and all(c in DIGITS for c in s)


def is_host(s):
    return s != "" and all(c in NAME_CHARS for c in s)


def parse_policy(line):
    """(rule, n, host or None), or None when the line is malformed."""
    f = line.split(" ")
    if len(f) not in (3, 5) or f[0] != "keep" or f[1] not in RULES or not is_digits(f[2]):
        return None
    if len(f) == 5 and (f[3] != "for" or not is_host(f[4])):
        return None
    return f[1], int(f[2]), f[4] if len(f) == 5 else None


def parse_backup(name):
    """(host, datetime), or None when the name is not a backup."""
    if not name.endswith(".tar"):
        return None
    parts = name[:-4].split("-")
    if len(parts) < 3:
        return None
    host, day, hm = "-".join(parts[:-2]), parts[-2], parts[-1]
    if not is_host(host) or len(day) != 8 or len(hm) != 4 or not is_digits(day) or not is_digits(hm):
        return None
    try:
        return host, datetime.datetime(int(day[:4]), int(day[4:6]), int(day[6:]), int(hm[:2]), int(hm[2:]))
    except ValueError:
        return None


def period(rule, d):
    if rule == "daily":
        return d
    if rule == "weekly":
        return d - datetime.timedelta(days=d.weekday())
    if rule == "monthly":
        return (d.year, d.month)
    return d.year


def plan_host(backups, rules, today):
    """[(file, reasons or None)] newest first."""
    out = []
    kept = {r: 0 for r in rules}
    last = {}
    for when, name in sorted(backups, reverse=True):
        d = when.date()
        if d > today:
            out.append((name, ["future"]))
            continue
        reasons = []
        for r in RULES:
            if r not in rules:
                continue
            n = rules[r]
            if r == "within":
                if (today - d).days <= n:
                    reasons.append(r)
                continue
            if kept[r] >= n:
                continue
            p = name if r == "last" else period(r, d)
            if r in last and last[r] == p:
                continue
            kept[r] += 1
            last[r] = p
            reasons.append(r)
        out.append((name, reasons or None))
    return out


def main():
    lines = sys.stdin.read().split("\n")
    today = datetime.date.fromisoformat(lines[0].split(" ")[1])
    name = lines[1][len("set "):]
    if not (1 <= len(name) <= 20 and all(c in NAME_CHARS for c in name)):
        print("error: bad set name")
        return
    policy = []
    for k, line in enumerate(lines[2:], start=3):
        if line == "":
            continue
        p = parse_policy(line)
        if p is None:
            print(f"error: bad policy line {k}")
            return
        policy.append(p)
    if not policy:
        print("error: no policy")
        return
    folder = f"backups/{name}/"
    if not os.path.isdir(folder):
        print("error: no such set")
        return
    hosts = {}
    for f in sorted(os.listdir(folder)):
        b = parse_backup(f)
        if b is None:
            print(f"ignore {f}")
        else:
            hosts.setdefault(b[0], []).append((b[1], f))
    print(f"plan for {name} as of {today}")
    kept = deleted = 0
    doomed = []
    for host in sorted(hosts):
        rules = {r: n for r, n, h in policy if h is None}
        rules.update({r: n for r, n, h in policy if h == host})
        print("== " + host)
        for f, reasons in plan_host(hosts[host], rules, today):
            if reasons is None:
                print("delete " + f)
                deleted += 1
                doomed.append(folder + f)
            else:
                print(f"keep {f} {','.join(reasons)}")
                kept += 1
    print(f"kept {kept}, deleted {deleted}")
    with open(f"plan/{name}.delete", "w") as out:
        out.write("".join(p + "\n" for p in doomed))
    print(f"wrote plan/{name}.delete {len(doomed)}")


main()
