def is_digits(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def parse_time(s):
    """Minutes since midnight, or -1 when s is not a time.

    Accepted: H:MM or HH:MM (hour 0-23), optionally followed by am/pm (hour 1-12)."""
    suffix = ""
    if len(s) >= 2 and (s[-2:] == "am" or s[-2:] == "pm"):
        suffix = s[-2:]
        s = s[:-2]
    colon = s.find(":")
    if colon < 1 or colon > 2 or len(s) != colon + 3:
        return -1
    hh = s[0:colon]
    mm = s[colon + 1:]
    if not is_digits(hh) or not is_digits(mm):
        return -1
    h = int(hh)
    m = int(mm)
    if m > 59:
        return -1
    if suffix == "":
        if h > 23:
            return -1
    else:
        if h < 1 or h > 12:
            return -1
        if h == 12:
            h = 0
        if suffix == "pm":
            h += 12
    return h * 60 + m


def two(n):
    s = str(n)
    if len(s) < 2:
        s = "0" + s
    return s


def format_time(m):
    """HH:MM for minutes since midnight."""
    return two(m // 60) + ":" + two(m % 60)


def span(start, end):
    return format_time(start) + "-" + format_time(end)


def duration(start, end):
    return end - start
