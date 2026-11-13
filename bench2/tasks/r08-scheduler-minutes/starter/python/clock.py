def is_digits(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def is_time(s):
    """A time is HH:MM with 00 <= HH <= 23 and 00 <= MM <= 59."""
    if len(s) != 5 or s[2] != ":":
        return False
    hh = s[0:2]
    mm = s[3:5]
    if not is_digits(hh) or not is_digits(mm):
        return False
    return int(hh) <= 23 and int(mm) <= 59


def to_minutes(t):
    return int(t[0:2]) * 60 + int(t[3:5])


def duration(start, end):
    """Minutes from start to end (both HH:MM)."""
    return to_minutes(end) - to_minutes(start)


def span(start, end):
    return start + "-" + end
