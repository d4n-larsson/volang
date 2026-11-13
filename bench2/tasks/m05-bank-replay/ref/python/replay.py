from bank import Bank
from parsing import show


def replay(entries):
    """entries: (line number, time, cmd, args) in input order. Returns output lines."""
    bank, out = Bank(), []
    for n, t, cmd, args in sorted(entries, key=lambda e: e[1]):
        reason = bank.apply(cmd, args)
        if reason:
            out.append(f"t={t} line {n}: {reason}")
    for a in sorted(bank.balance):
        out.append(f"{a} {show(bank.balance[a])}" + (" closed" if a in bank.closed else ""))
    out.append(f"fees {show(bank.fees)} interest {show(bank.interest)}")
    return out
