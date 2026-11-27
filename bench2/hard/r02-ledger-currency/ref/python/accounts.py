"""Opening, closing and renaming accounts, and their balances."""

import parse
import rates
from model import BASE, Account


def native_balance(account):
    total = 0
    for p in account.postings:
        total += p.amount
    return total


def covered(ledger, name):
    """The accounts named `name` or below it (`name:...`), in byte order of their names."""
    out = []
    for n in sorted(ledger.accounts):
        if n == name or n.startswith(name + ":"):
            out.append(ledger.accounts[n])
    return out


def book_balance(account):
    """The account's base value at the rates of its postings."""
    total = 0
    for p in account.postings:
        total += p.base
    return total


def current_value(ledger, account):
    """The account's balance in base cents at the latest rate of its currency."""
    bal = native_balance(account)
    if account.currency == BASE:
        return bal
    return rates.to_base(bal, rates.latest_rate(ledger, account.currency))


def subtree_total(ledger, name):
    total = 0
    for a in covered(ledger, name):
        total += current_value(ledger, a)
    return total


def subtree_currencies(ledger, name):
    """[(currency, native total)] of the foreign currencies below name with a non-zero total, by code."""
    totals = {}
    for a in covered(ledger, name):
        if a.currency != BASE:
            totals[a.currency] = totals.get(a.currency, 0) + native_balance(a)
    return [(c, totals[c]) for c in sorted(totals) if totals[c] != 0]


def open_account(ledger, name, kind, currency):
    if not parse.valid_account_name(name):
        return "error: bad account name " + name
    if not parse.valid_type(kind):
        return "error: bad type " + kind
    if not parse.valid_currency(currency):
        return "error: bad currency " + currency
    if name in ledger.accounts:
        return "error: account " + name + " exists"
    ledger.accounts[name] = Account(name, kind, currency)
    if currency == BASE:
        return "opened " + name + " (" + kind + ")"
    return "opened " + name + " (" + kind + ", " + currency + ")"


def close_account(ledger, name):
    if name not in ledger.accounts:
        return "error: unknown account " + name
    a = ledger.accounts[name]
    if a.closed:
        return "error: account " + name + " is closed"
    bal = native_balance(a)
    if bal != 0:
        return "error: account " + name + " has balance " + rates.native_text(bal, a.currency)
    a.closed = True
    return "closed " + name


def rename_account(ledger, old, new):
    if old not in ledger.accounts:
        return "error: unknown account " + old
    if not parse.valid_account_name(new):
        return "error: bad account name " + new
    if new in ledger.accounts:
        return "error: account " + new + " exists"
    a = ledger.accounts.pop(old)
    a.name = new
    ledger.accounts[new] = a
    return "renamed " + old + " to " + new
