from parsing import money


class Customer:
    def __init__(self, cid, limit, terms, tax):
        self.id, self.limit, self.terms, self.tax = cid, limit, terms, tax
        self.credit = 0
        self.written_off = 0


class Invoice:
    def __init__(self, num, cust, issue, due, total):
        self.num, self.cust, self.issue, self.due, self.total = num, cust, issue, due, total
        self.fee = self.paid = self.credits = self.written = 0
        self.status = "open"

    def balance(self):
        return self.total + self.fee - self.paid - self.credits - self.written

    def settle(self):
        if self.status == "open" and self.balance() == 0:
            self.status = "closed"


def half_up(p, q):
    return (p + q // 2) // q


def invoice_total(items, tax):
    total = 0
    for qty, price in items:
        gross = qty * price
        net = gross - (gross * 5 // 100 if qty >= 10 else 0)
        total += net + half_up(net * tax, 10000)
    return total


class Ledger:
    def __init__(self):
        self.customers = {}
        self.invoices = {}
        self.day = 0

    def overdue(self, inv):
        return self.day - inv.due

    def open_of(self, cid):
        """The customer's open invoices in allocation order: due day, then number."""
        return sorted((i for i in self.invoices.values() if i.cust == cid and i.status == "open"),
                      key=lambda i: (i.due, i.num))

    def open_balance(self, cid):
        return sum(i.balance() for i in self.open_of(cid))

    def run(self, cmd, args, out):
        getattr(self, "do_" + cmd)(*args, out=out)

    def do_customer(self, cid, limit, terms, tax, out):
        if cid in self.customers:
            out.append(f"error: customer {cid} exists")
            return
        self.customers[cid] = Customer(cid, limit, terms, tax)
        out.append(f"customer {cid} added")

    def do_terms(self, cid, terms, out):
        c = self.customers.get(cid)
        if c is None:
            out.append(f"error: no customer {cid}")
            return
        c.terms = terms
        out.append(f"terms {cid} {terms}")

    def do_invoice(self, cid, items, out):
        c = self.customers.get(cid)
        if c is None:
            out.append(f"error: no customer {cid}")
            return
        total = invoice_total(items, c.tax)
        if any(self.overdue(i) > 60 for i in self.open_of(cid)):
            out.append(f"error: {cid} on hold")
            return
        exposure = self.open_balance(cid) + total - c.credit
        if exposure > c.limit:
            out.append(f"error: {cid} over limit by {money(exposure - c.limit)}")
            return
        num = len(self.invoices) + 1
        inv = Invoice(num, cid, self.day, self.day + c.terms, total)
        self.invoices[num] = inv
        use = min(c.credit, total)
        c.credit -= use
        inv.paid += use
        inv.settle()
        out.append(f"invoice #{num} {cid} total {money(total)} balance {money(inv.balance())} due {inv.due}")

    def do_pay(self, cid, amt, nums, out):
        if cid not in self.customers:
            out.append(f"error: no customer {cid}")
            return
        listed = []
        for n in nums:
            inv = self.invoices.get(n)
            if inv is None:
                out.append(f"error: no invoice {n}")
                return
            if inv.cust != cid or inv.status != "open":
                out.append(f"error: invoice {n} not open for {cid}")
                return
            listed.append(inv)
        order = listed + [i for i in self.open_of(cid) if i.num not in nums]
        rest, parts = amt, []
        for inv in order:
            take = min(rest, inv.balance())
            if take > 0:
                inv.paid += take
                inv.settle()
                rest -= take
                parts.append(f"#{inv.num} {money(take)}")
        if rest > 0:
            self.customers[cid].credit += rest
            parts.append(f"credit {money(rest)}")
        out.append(f"paid {cid} {money(amt)}: " + ", ".join(parts))

    def find_open(self, n, out):
        inv = self.invoices.get(n)
        if inv is None:
            out.append(f"error: no invoice {n}")
            return None
        if inv.status != "open":
            out.append(f"error: invoice {n} is {inv.status}")
            return None
        return inv

    def do_credit(self, n, amt, out):
        inv = self.find_open(n, out)
        if inv is None:
            return
        if amt > inv.balance():
            out.append(f"error: credit exceeds balance {money(inv.balance())} of #{n}")
            return
        inv.credits += amt
        inv.settle()
        out.append(f"credit #{n} {money(amt)} balance {money(inv.balance())}")

    def do_void(self, n, out):
        inv = self.find_open(n, out)
        if inv is None:
            return
        if inv.paid > 0:
            out.append(f"error: invoice {n} has payments")
            return
        inv.written += inv.balance()
        inv.status = "void"
        out.append(f"void #{n}")

    def do_writeoff(self, n, out):
        inv = self.find_open(n, out)
        if inv is None:
            return
        if self.overdue(inv) <= 90:
            out.append(f"error: invoice {n} is {self.overdue(inv)} days overdue")
            return
        amt = inv.balance()
        inv.written += amt
        self.customers[inv.cust].written_off += amt
        inv.status = "written-off"
        out.append(f"write off #{n} {money(amt)}")

    def do_refund(self, cid, amt, out):
        c = self.customers.get(cid)
        if c is None:
            out.append(f"error: no customer {cid}")
            return
        if amt > c.credit:
            out.append(f"error: {cid} credit is only {money(c.credit)}")
            return
        c.credit -= amt
        out.append(f"refund {cid} {money(amt)} credit {money(c.credit)}")

    def do_day(self, d, out):
        if d < self.day:
            out.append(f"error: day {self.day}")
            return
        self.day = d
        for n in sorted(self.invoices):
            inv = self.invoices[n]
            if inv.status == "open" and inv.fee == 0 and self.overdue(inv) > 30:
                inv.fee = max(500, half_up(inv.balance() * 150, 10000))
                out.append(f"fee #{n} {money(inv.fee)}")

    def do_statement(self, cid, out):
        import reports
        reports.statement(self, cid, out)

    def do_aging(self, out):
        import reports
        reports.aging(self, out)
