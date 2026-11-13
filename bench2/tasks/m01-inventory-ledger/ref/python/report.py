def money(c):
    sign = "-" if c < 0 else ""
    c = abs(c)
    return f"{sign}{c // 100}.{c % 100:02d}"


def report(ledger):
    out = ["REPORT"]
    total = 0
    for sku in sorted(ledger.lots):
        lots = ledger.lots[sku]
        value = sum(q * c for q, c in lots)
        total += value
        out.append(f"{sku} {sum(q for q, _ in lots)} {money(value)}")
    profit = ledger.revenue - ledger.cogs - ledger.loss
    out.append(f"revenue {money(ledger.revenue)} cogs {money(ledger.cogs)} loss {money(ledger.loss)} "
               f"profit {money(profit)} stock {money(total)}")
    return out
