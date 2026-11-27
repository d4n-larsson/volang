"""Checks on command arguments."""


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def to_int(s):
    # Only called on strings that passed is_number.
    return int(s)


def shape_ok(args, count, numbers):
    """True when there are exactly `count` arguments and those at the positions in `numbers` are numbers."""
    if len(args) != count:
        return False
    for i in numbers:
        if not is_number(args[i]):
            return False
    return True
