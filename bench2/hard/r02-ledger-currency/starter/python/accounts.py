"""Opening, closing and renaming accounts, and their balances."""

import parse
from fmt import money
from model import Account


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


def subtree_total(ledger, name):
    total = 0
    for a in covered(ledger, name):
        total += native_balance(a)
    return total


def open_account(ledger, name, kind):
    if not parse.valid_account_name(name):
        return "error: bad account name " + name
    if not parse.valid_type(kind):
        return "error: bad type " + kind
    if name in ledger.accounts:
        return "error: account " + name + " exists"
    ledger.accounts[name] = Account(name, kind)
    return "opened " + name + " (" + kind + ")"


def close_account(ledger, name):
    if name not in ledger.accounts:
        return "error: unknown account " + name
    a = ledger.accounts[name]
    if a.closed:
        return "error: account " + name + " is closed"
    bal = native_balance(a)
    if bal != 0:
        return "error: account " + name + " has balance " + money(bal)
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
