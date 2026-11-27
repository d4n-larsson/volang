"""Read-only reports over the ledger."""

import accounts
import rates
from fmt import indent, money, pad_left, pad_right, tree_key
from journal import positive_total
from model import ACCOUNT_TYPES, BASE


def balance_lines(ledger, name):
    if not accounts.covered(ledger, name):
        return ["error: unknown account " + name]
    out = ["balance " + name + ": " + money(accounts.subtree_total(ledger, name))]
    for currency, total in accounts.subtree_currencies(ledger, name):
        out.append("  " + currency + " " + money(total))
    return out


def by_day(postings):
    """Postings by day; on one day in the order they were entered."""
    return sorted(postings, key=lambda p: (p.day, p.seq))


def statement_line(p, running):
    return ("  " + pad_left(str(p.day), 4) + " " + pad_right(p.txn_id, 8) + " "
            + pad_left(money(p.amount), 10) + " " + pad_left(money(running), 10) + "  " + p.desc)


def statement_lines(ledger, name):
    if name not in ledger.accounts:
        return ["error: unknown account " + name]
    a = ledger.accounts[name]
    if a.currency == BASE:
        out = ["statement " + name + " (" + a.kind + ")"]
    else:
        out = ["statement " + name + " (" + a.kind + ", " + a.currency + ")"]
    if not a.postings:
        out.append("  no postings")
    running = 0
    for p in by_day(a.postings):
        running += p.amount
        out.append(statement_line(p, running))
    if a.currency == BASE:
        out.append("  closing balance " + money(running))
    else:
        out.append("  closing balance " + rates.native_text(running, a.currency)
                   + " (book " + money(accounts.book_balance(a)) + " " + BASE + ")")
    return out


def trial_lines(ledger):
    out = []
    debits = 0
    credits = 0
    for kind in ACCOUNT_TYPES:
        rows = []
        for n in sorted(ledger.accounts):
            a = ledger.accounts[n]
            if a.kind != kind:
                continue
            bal = accounts.book_balance(a)
            if bal == 0:
                continue
            if bal > 0:
                debits += bal
            else:
                credits -= bal
            rows.append("  " + n + " " + money(bal))
        if rows:
            out.append(kind)
            out.extend(rows)
    out.append("total debits " + money(debits) + " credits " + money(credits))
    return out


def range_sum(account, start, end):
    total = 0
    for p in account.postings:
        if start <= p.day <= end:
            total += p.base
    return total


def income_lines(ledger, start, end):
    if start > end:
        return ["error: bad range"]
    out = []
    earned = 0
    spent = 0
    for kind in ["income", "expense"]:
        for n in sorted(ledger.accounts):
            a = ledger.accounts[n]
            if a.kind != kind:
                continue
            s = range_sum(a, start, end)
            if s == 0:
                continue
            if kind == "income":
                earned -= s
                out.append("income " + n + " " + money(-s))
            else:
                spent += s
                out.append("expense " + n + " " + money(s))
    out.append("net income " + money(earned - spent))
    return out


def account_lines(ledger):
    if not ledger.accounts:
        return ["no accounts"]
    out = []
    for n in sorted(ledger.accounts):
        a = ledger.accounts[n]
        line = n + " " + a.kind + " " + rates.native_text(accounts.native_balance(a), a.currency)
        if a.closed:
            line += " closed"
        out.append(line)
    return out


def tree_lines(ledger):
    if not ledger.accounts:
        return ["no accounts"]
    nodes = set()
    for n in ledger.accounts:
        parts = n.split(":")
        for i in range(1, len(parts) + 1):
            nodes.add(":".join(parts[:i]))
    out = []
    for node in sorted(nodes, key=tree_key):
        parts = node.split(":")
        total = accounts.subtree_total(ledger, node)
        out.append(indent(len(parts) - 1) + parts[-1] + " " + money(total))
    return out


def journal_lines(ledger, start, end):
    if start > end:
        return ["error: bad range"]
    picked = [t for t in ledger.order if start <= t.day <= end]
    picked.sort(key=lambda t: t.day)
    if not picked:
        return ["no transactions"]
    out = []
    for t in picked:
        line = str(t.day) + " " + t.txn_id + " " + t.desc + " " + money(positive_total(t))
        if t.voided_by != "":
            line += " (void)"
        out.append(line)
    return out


def revalue_lines(ledger, day):
    rows = []
    for n in sorted(ledger.accounts):
        a = ledger.accounts[n]
        if a.currency == BASE:
            continue
        native = 0
        book = 0
        for p in a.postings:
            if p.day <= day:
                native += p.amount
                book += p.base
        if native == 0 and book == 0:
            continue
        # every posting dated DAY or earlier needed a rate on its day, so there is one on DAY
        rows.append((a, native, book, rates.to_base(native, rates.rate_on(ledger, a.currency, day))))
    out = []
    gain = 0
    for a, native, book, value in rows:
        gain += value - book
        out.append(a.name + " " + rates.native_text(native, a.currency) + ": book " + money(book)
                   + ", value " + money(value) + ", gain " + money(value - book))
    out.append("unrealized gain " + money(gain))
    return out
