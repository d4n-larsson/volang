"""Checks on command arguments."""

REASONS = ["count", "damage", "found", "loss"]


def is_number(s):
    if len(s) == 0:
        return False
    for c in s:
        if c < "0" or c > "9":
            return False
    return True


def is_positive(s):
    return is_number(s) and int(s) >= 1


def is_delta(s):
    """+N or -N with N at least 1."""
    if len(s) < 2:
        return False
    if s[0] != "+" and s[0] != "-":
        return False
    return is_positive(s[1:])


def delta_value(s):
    if s[0] == "-":
        return -int(s[1:])
    return int(s[1:])


def is_reason(word):
    return word in REASONS
