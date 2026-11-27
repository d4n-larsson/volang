import os
import sys

REQUIRED = ["id", "date", "account", "amount"]
DIGITS = "0123456789"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ID_CHARS = set(UPPER + UPPER.lower() + DIGITS + "-")


def all_digits(s):
    return s != "" and all(c in DIGITS for c in s)


def valid_date(s):
    if len(s) != 10 or s[4] != "-" or s[7] != "-":
        return False
    y, m, d = s[0:4], s[5:7], s[8:10]
    if not (all_digits(y) and all_digits(m) and all_digits(d)):
        return False
    y, m, d = int(y), int(m), int(d)
    if m < 1 or m > 12 or d < 1:
        return False
    leap = y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)
    days = [31, 29 if leap else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31][m - 1]
    return d <= days


def parse_cents(s):
    neg = s.startswith("-")
    body = s[1:] if neg else s
    whole, frac = body, "00"
    if "." in body:
        whole, frac = body.split(".", 1)
        if len(frac) != 2 or not all_digits(frac):
            return None
    if not all_digits(whole):
        return None
    v = int(whole) * 100 + int(frac)
    return -v if neg else v


def money(c):
    sign = "-" if c < 0 else ""
    c = abs(c)
    return f"{sign}{c // 100}.{c % 100:02d}"


def valid_batch(b):
    return 1 <= len(b) <= 20 and all(c in DIGITS or c in UPPER.lower() or c == "-" for c in b)


def main():
    lines = sys.stdin.read().split("\n")
    batch, lo, hi = lines[0].split(" ")
    if not valid_batch(batch):
        print("error: bad batch name")
        return
    errors, rows, seen_names, seen_ids = [], [], set(), set()

    def report(line):
        print(line)
        errors.append(line)

    for name in lines[1:]:
        if name == "":
            continue
        # read every listed file up front; the checks below decide what to use
        try:
            with open("inbox/" + name) as f:
                f.read()
        except OSError:
            pass
        listed_before = name in seen_names
        seen_names.add(name)
        if name.startswith("/") or ".." in name.split("/"):
            report(f"reject {name}: outside inbox")
            continue
        if listed_before:
            report(f"reject {name}: duplicate file")
            continue
        path = "inbox/" + name
        if not os.path.isfile(path):
            report(f"reject {name}: not found")
            continue
        with open(path) as f:
            text = f.read()
        numbered = [(i + 1, l) for i, l in enumerate(text.split("\n")) if l != ""]
        if not numbered:
            report(f"reject {name}: no header")
            continue
        header = [c.strip(" ") for c in numbered[0][1].split(";")]
        dup = next((c for i, c in enumerate(header) if c in header[:i]), None)
        if dup is not None:
            report(f"reject {name}: duplicate column {dup}")
            continue
        missing = next((c for c in REQUIRED if c not in header), None)
        if missing is not None:
            report(f"reject {name}: missing column {missing}")
            continue
        data = numbered[1:]
        if len(numbered) > 1 and numbered[-1][1].startswith("#"):
            n = int(numbered[-1][1][len("#rows="):])
            data = numbered[1:-1]
            if n != len(data):
                report(f"reject {name}: trailer says {n}, found {len(data)}")
                continue
        col = {c: i for i, c in enumerate(header)}
        for no, line in data:
            fields = [x.strip(" ") for x in line.split(";")]
            where = f"{name}:{no}: "
            if len(fields) != len(header):
                report(where + f"expected {len(header)} fields")
                continue
            tid, date, acct, amount = (fields[col[c]] for c in REQUIRED)
            if not (1 <= len(tid) <= 12 and all(c in ID_CHARS for c in tid)):
                report(where + f"bad id {tid}")
                continue
            if not valid_date(date):
                report(where + f"bad date {date}")
                continue
            if date < lo or date > hi:
                report(where + f"outside period {date}")
                continue
            if not (len(acct) == 6 and all(c in UPPER for c in acct[:2]) and all_digits(acct[2:])):
                report(where + f"bad account {acct}")
                continue
            cents = parse_cents(amount)
            if cents is None:
                report(where + f"bad amount {amount}")
                continue
            if tid in seen_ids:
                report(where + f"duplicate id {tid}")
                continue
            seen_ids.add(tid)
            rows.append((date, acct, tid, cents))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))
    ledger = [f"{d} {a} {i} {money(c)}" for d, a, i, c in rows]
    balances = {}
    for _, a, _, c in rows:
        n, s = balances.get(a, (0, 0))
        balances[a] = (n + 1, s + c)
    print("== ledger")
    for l in ledger:
        print(l)
    print("== balances")
    for a in sorted(balances):
        print(f"{a} {balances[a][0]} {money(balances[a][1])}")
    with open(f"out/{batch}.ledger", "w") as f:
        f.write("".join(l + "\n" for l in ledger))
    with open(f"out/{batch}.errors", "w") as f:
        f.write("".join(l + "\n" for l in errors))
    print(f"wrote out/{batch}.ledger {len(ledger)}")
    print(f"wrote out/{batch}.errors {len(errors)}")


main()
