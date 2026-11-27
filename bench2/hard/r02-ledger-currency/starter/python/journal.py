"""Entering transactions: post, void, show."""

import parse
from fmt import money
from model import Posting, Transaction, new_seq


def locked_error(ledger, day):
    if day <= ledger.locked_through:
        return "error: books locked through " + str(ledger.locked_through)
    return ""


def read_specs(ledger, specs):
    """Checks the `NAME=AMOUNT` / `NAME=` words in order.

    Returns (error, entries) with entries a list of [account, amount or None].
    """
    entries = []
    seen = set()
    for spec in specs:
        has_eq, name, text = parse.split_spec(spec)
        if not has_eq:
            return "error: bad posting " + spec, []
        amount = None
        if text != "":
            ok, cents = parse.parse_amount(text)
            if not ok:
                return "error: bad posting " + spec, []
            amount = cents
        if name not in ledger.accounts:
            return "error: unknown account " + name, []
        account = ledger.accounts[name]
        if account.closed:
            return "error: account " + name + " is closed", []
        if name in seen:
            return "error: account " + name + " posted twice", []
        if amount == 0:
            return "error: zero posting " + name, []
        seen.add(name)
        entries.append([account, amount])
    return "", entries


def positive_total(txn):
    total = 0
    for p in txn.postings:
        if p.amount > 0:
            total += p.amount
    return total


def record(ledger, txn, entries):
    for account, amount in entries:
        p = Posting(account, amount, txn.txn_id, txn.day, txn.desc, new_seq(ledger))
        txn.postings.append(p)
        account.postings.append(p)
    ledger.txns[txn.txn_id] = txn
    ledger.order.append(txn)


def post(ledger, txn_id, day, desc, specs):
    if not parse.valid_txn_id(txn_id):
        return "error: bad id " + txn_id
    if txn_id in ledger.txns:
        return "error: transaction " + txn_id + " exists"
    locked = locked_error(ledger, day)
    if locked:
        return locked
    error, entries = read_specs(ledger, specs)
    if error:
        return error
    blanks = [e for e in entries if e[1] is None]
    if len(blanks) > 1:
        return "error: more than one balancing posting"
    total = 0
    for e in entries:
        if e[1] is not None:
            total += e[1]
    if blanks:
        if total == 0:
            return "error: zero posting " + blanks[0][0].name
        blanks[0][1] = -total
    elif total != 0:
        return "error: transaction " + txn_id + " unbalanced by " + money(total)
    txn = Transaction(txn_id, day, desc)
    record(ledger, txn, entries)
    return "posted " + txn_id + ": " + str(len(entries)) + " postings, total " + money(positive_total(txn))


def void(ledger, txn_id, day):
    if txn_id not in ledger.txns:
        return "error: unknown transaction " + txn_id
    txn = ledger.txns[txn_id]
    if txn.reverses != "":
        return "error: transaction " + txn_id + " is a reversal"
    if txn.voided_by != "":
        return "error: transaction " + txn_id + " already voided"
    if day < txn.day:
        return "error: void before day " + str(txn.day)
    locked = locked_error(ledger, day)
    if locked:
        return locked
    for p in txn.postings:
        if p.account.closed:
            return "error: account " + p.account.name + " is closed"
    rev = Transaction("rev-" + txn_id, day, "void:" + txn.desc)
    rev.reverses = txn_id
    record(ledger, rev, [[p.account, -p.amount] for p in txn.postings])
    txn.voided_by = rev.txn_id
    return "voided " + txn_id + " on day " + str(day)


def show(ledger, txn_id):
    if txn_id not in ledger.txns:
        return ["error: unknown transaction " + txn_id]
    txn = ledger.txns[txn_id]
    out = [txn.txn_id + " day " + str(txn.day) + " " + txn.desc]
    for p in txn.postings:
        out.append("  " + p.account.name + " " + money(p.amount))
    if txn.voided_by != "":
        out.append("  voided by " + txn.voided_by)
    if txn.reverses != "":
        out.append("  reverses " + txn.reverses)
    return out


def lock(ledger, day):
    if day < ledger.locked_through:
        return "error: already locked through " + str(ledger.locked_through)
    ledger.locked_through = day
    return "locked through " + str(day)
