from parsing import money


def statement(led, cid, out):
    c = led.customers.get(cid)
    if c is None:
        out.append(f"error: no customer {cid}")
        return
    out.append(f"statement {cid} day {led.day}")
    for inv in led.open_of(cid):
        line = f"#{inv.num} due {inv.due} balance {money(inv.balance())}"
        if led.overdue(inv) > 0:
            line += f" overdue {led.overdue(inv)}"
        out.append(line)
    out.append(f"open {money(led.open_balance(cid))} credit {money(c.credit)} written off {money(c.written_off)}")


def bucket(days):
    if days <= 0:
        return 0
    if days <= 30:
        return 1
    if days <= 60:
        return 2
    if days <= 90:
        return 3
    return 4


def aging(led, out):
    out.append(f"aging day {led.day}")
    total = [0] * 5
    for cid in sorted(led.customers, key=lambda s: s.encode()):
        row = [0] * 5
        for inv in led.open_of(cid):
            row[bucket(led.overdue(inv))] += inv.balance()
        if sum(row) > 0:
            out.append(cid + " " + " ".join(money(v) for v in row))
            total = [a + b for a, b in zip(total, row)]
    out.append("total " + " ".join(money(v) for v in total))


def final(led, out):
    out.append("invoices:")
    for n in sorted(led.invoices):
        i = led.invoices[n]
        out.append(f"#{n} {i.cust} {i.status} total {money(i.total)} fee {money(i.fee)} paid {money(i.paid)} "
                   f"credits {money(i.credits)} balance {money(i.balance())}")
