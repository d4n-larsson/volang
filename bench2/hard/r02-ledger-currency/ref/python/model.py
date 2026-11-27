"""The records the whole ledger shares."""

ACCOUNT_TYPES = ["asset", "liability", "equity", "income", "expense"]
BASE = "USD"


class Account:
    """An account. Its postings are kept in the order they were entered."""

    def __init__(self, name, kind, currency):
        self.name = name
        self.kind = kind
        self.currency = currency
        self.closed = False
        self.postings = []


class Posting:
    """One line of a transaction: an amount (in cents of the account's currency) on one account,
    and its value in base cents, fixed when it was posted."""

    def __init__(self, account, amount, base, txn_id, day, desc, seq):
        self.account = account
        self.amount = amount
        self.base = base
        self.txn_id = txn_id
        self.day = day
        self.desc = desc
        self.seq = seq


class Transaction:
    def __init__(self, txn_id, day, desc):
        self.txn_id = txn_id
        self.day = day
        self.desc = desc
        self.postings = []
        # the id of the reversal that voided this transaction, or ""
        self.voided_by = ""
        # for a reversal: the id of the transaction it reverses, or ""
        self.reverses = ""


class Ledger:
    def __init__(self):
        self.accounts = {}
        self.txns = {}
        # transactions in the order they were entered
        self.order = []
        # the books are closed through this day (-1: nothing is locked)
        self.locked_through = -1
        self.next_seq = 0
        # currency -> {day: rate in ten-thousandths}
        self.rates = {}


def new_seq(ledger):
    ledger.next_seq += 1
    return ledger.next_seq
