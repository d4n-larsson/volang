import sys

from accounts import Accounts
from book import Engine
from parse import parse


def main():
    out = []
    acc = Accounts()
    eng = Engine(acc, out)
    used, symbols = set(), set()
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        c = parse(line)
        if c is None:
            out.append(f"invalid line {n}")
            continue
        cmd = c["cmd"]
        if cmd == "blank":
            continue
        if cmd == "book":
            out.extend(eng.book(c["symbol"]))
            continue
        if cmd == "cancel":
            why = eng.cancel(c["id"])
            if why:
                out.append(f"reject {c['id']} {why}")
            continue
        if cmd == "modify":
            why, sym = eng.modify(c)
            if why:
                out.append(f"reject {c['id']} {why}")
            else:
                eng.triggers(sym)
            continue
        if c["id"] in used:
            out.append(f"reject {c['id']} duplicate id")
            continue
        used.add(c["id"])
        why = {"limit": eng.limit, "market": eng.market, "stop": eng.stop}[cmd](c)
        if why:
            out.append(f"reject {c['id']} {why}")
            continue
        symbols.add(c["symbol"])
        eng.triggers(c["symbol"])
    for s in sorted(symbols):
        out.extend(eng.book(s))
    out.extend(acc.report())
    sys.stdout.write("".join(l + "\n" for l in out))


main()
