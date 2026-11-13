import sys

lines = sys.stdin.read().split("\n")
asof = lines[0]
print("Statement as of " + asof)
grand = 0
for name in lines[1:]:
    if not name:
        continue
    print("== " + name)
    try:
        with open(f"ledger/{name}.txt") as f:
            text = f.read()
    except OSError:
        print("no ledger")
        continue
    entries = [line.split(" ", 2) for line in text.split("\n") if line]
    shown = sorted((e for e in entries if e[0] <= asof), key=lambda e: e[0])
    balance = 0
    for date, amount, desc in shown:
        a = int(amount)
        balance += a
        print(f"{date} {'+' if a >= 0 else ''}{a} {desc}")
    print(f"balance {balance} ({len(shown)} entries, {len(entries) - len(shown)} later)")
    grand += balance
print(f"grand total {grand}")
