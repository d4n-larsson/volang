"""Reading the words of a command."""

from model import ACCOUNT_TYPES


def is_number(s):
    """A non-empty string of the digits 0-9."""
    if s == "":
        return False
    for c in s:
        if c < "0" or c > "9":
            return False
    return True


def digits_value(s):
    """The value of a string that passed is_number."""
    n = 0
    for c in s:
        n = n * 10 + (ord(c) - ord("0"))
    return n


def parse_amount(s):
    """An amount: optional `-`, digits, optionally `.` and one or two digits.

    Returns (True, cents) or (False, 0).
    """
    negative = s.startswith("-")
    body = s[1:] if negative else s
    whole, dot, frac = body.partition(".")
    if not is_number(whole):
        return False, 0
    cents = digits_value(whole) * 100
    if dot:
        if len(frac) < 1 or len(frac) > 2 or not is_number(frac):
            return False, 0
        if len(frac) == 1:
            frac = frac + "0"
        cents += digits_value(frac)
    if negative:
        cents = -cents
    return True, cents


def parse_rate(s):
    """A rate: digits, optionally `.` and one to four digits, in ten-thousandths; (True, value) or (False, 0)."""
    whole, dot, frac = s.partition(".")
    if not is_number(whole):
        return False, 0
    value = digits_value(whole) * 10000
    if dot:
        if len(frac) < 1 or len(frac) > 4 or not is_number(frac):
            return False, 0
        while len(frac) < 4:
            frac = frac + "0"
        value += digits_value(frac)
    return True, value


def valid_currency(code):
    if len(code) != 3:
        return False
    for c in code:
        if c < "A" or c > "Z":
            return False
    return True


def is_lower_word(s):
    """Lower-case letters and digits, at least one."""
    if s == "":
        return False
    for c in s:
        if not (("a" <= c <= "z") or ("0" <= c <= "9")):
            return False
    return True


def valid_account_name(name):
    """Segments of lower-case letters and digits joined by `:`."""
    for segment in name.split(":"):
        if not is_lower_word(segment):
            return False
    return True


def valid_txn_id(txn_id):
    return is_lower_word(txn_id)


def valid_type(kind):
    return kind in ACCOUNT_TYPES


def split_spec(spec):
    """`NAME=AMOUNT` into (True, NAME, AMOUNT text); (False, "", "") without `=`."""
    at = spec.find("=")
    if at < 0:
        return False, "", ""
    return True, spec[:at], spec[at + 1:]
