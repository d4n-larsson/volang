# Checking and reading the words of a command.

from model import MINUTES_PER_DAY


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def number(s):
    # only called on strings that passed is_number
    return int(s)


def parse_clock(s):
    """Minutes since midnight for `HH:MM` (00:00 to 23:59), or -1."""
    if len(s) != 5 or s[2] != ":":
        return -1
    hours, minutes = s[0:2], s[3:5]
    if not is_number(hours) or not is_number(minutes):
        return -1
    h = number(hours)
    m = number(minutes)
    if h > 23 or m > 59:
        return -1
    return h * 60 + m


def parse_moment(day, clock):
    """The time `DAY HH:MM` given as two words, or -1."""
    if not is_number(day):
        return -1
    c = parse_clock(clock)
    if c < 0:
        return -1
    return number(day) * MINUTES_PER_DAY + c


def parse_length(s):
    """A meeting length in minutes, 1 to 1440, or -1."""
    if not is_number(s):
        return -1
    n = number(s)
    if n < 1 or n > MINUTES_PER_DAY:
        return -1
    return n


def parse_offset(s):
    """Minutes of a UTC offset `+HH:MM` or `-HH:MM`, from -12:00 to +14:00, or None."""
    if len(s) != 6 or (s[0] != "+" and s[0] != "-"):
        return None
    c = parse_clock(s[1:])
    if c < 0:
        return None
    if s[0] == "-":
        c = -c
    if c < -12 * 60 or c > 14 * 60:
        return None
    return c


def parse_capacity(s):
    if not is_number(s):
        return -1
    n = number(s)
    if n < 1:
        return -1
    return n
