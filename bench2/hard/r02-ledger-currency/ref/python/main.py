import sys

import accounts
import journal
import parse
import rates
import reports
from model import BASE, Ledger

BAD = ["error: bad command"]


def cmd_open(ledger, a):
    if len(a) != 2 and len(a) != 3:
        return BAD
    currency = a[2] if len(a) == 3 else BASE
    return [accounts.open_account(ledger, a[0], a[1], currency)]


def cmd_rate(ledger, a):
    if len(a) != 3 or not parse.is_number(a[1]):
        return BAD
    ok, rate = parse.parse_rate(a[2])
    if not ok or rate == 0:
        return BAD
    if not parse.valid_currency(a[0]):
        return ["error: bad currency " + a[0]]
    return [rates.set_rate(ledger, a[0], parse.digits_value(a[1]), rate)]


def cmd_revalue(ledger, a):
    if len(a) != 1 or not parse.is_number(a[0]):
        return BAD
    return reports.revalue_lines(ledger, parse.digits_value(a[0]))


def cmd_close(ledger, a):
    if len(a) != 1:
        return BAD
    return [accounts.close_account(ledger, a[0])]


def cmd_rename(ledger, a):
    if len(a) != 2:
        return BAD
    return [accounts.rename_account(ledger, a[0], a[1])]


def cmd_post(ledger, a):
    if len(a) < 5 or not parse.is_number(a[1]):
        return BAD
    return [journal.post(ledger, a[0], parse.digits_value(a[1]), a[2], a[3:])]


def cmd_void(ledger, a):
    if len(a) != 2 or not parse.is_number(a[1]):
        return BAD
    return [journal.void(ledger, a[0], parse.digits_value(a[1]))]


def cmd_show(ledger, a):
    if len(a) != 1:
        return BAD
    return journal.show(ledger, a[0])


def cmd_lock(ledger, a):
    if len(a) != 1 or not parse.is_number(a[0]):
        return BAD
    return [journal.lock(ledger, parse.digits_value(a[0]))]


def cmd_balance(ledger, a):
    if len(a) != 1:
        return BAD
    return reports.balance_lines(ledger, a[0])


def cmd_statement(ledger, a):
    if len(a) != 1:
        return BAD
    return reports.statement_lines(ledger, a[0])


def cmd_trial(ledger, a):
    if len(a) != 0:
        return BAD
    return reports.trial_lines(ledger)


def two_days(a):
    return len(a) == 2 and parse.is_number(a[0]) and parse.is_number(a[1])


def cmd_income(ledger, a):
    if not two_days(a):
        return BAD
    return reports.income_lines(ledger, parse.digits_value(a[0]), parse.digits_value(a[1]))


def cmd_journal(ledger, a):
    if not two_days(a):
        return BAD
    return reports.journal_lines(ledger, parse.digits_value(a[0]), parse.digits_value(a[1]))


def cmd_accounts(ledger, a):
    if len(a) != 0:
        return BAD
    return reports.account_lines(ledger)


def cmd_tree(ledger, a):
    if len(a) != 0:
        return BAD
    return reports.tree_lines(ledger)


COMMANDS = {
    "open": cmd_open,
    "close": cmd_close,
    "rename": cmd_rename,
    "post": cmd_post,
    "void": cmd_void,
    "show": cmd_show,
    "lock": cmd_lock,
    "balance": cmd_balance,
    "statement": cmd_statement,
    "trial": cmd_trial,
    "income": cmd_income,
    "journal": cmd_journal,
    "accounts": cmd_accounts,
    "tree": cmd_tree,
    "rate": cmd_rate,
    "revalue": cmd_revalue,
}


def run(ledger, words):
    handler = COMMANDS.get(words[0])
    if handler is None:
        return ["error: unknown command " + words[0]]
    return handler(ledger, words[1:])


def main():
    ledger = Ledger()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(ledger, words):
            print(out)


main()
