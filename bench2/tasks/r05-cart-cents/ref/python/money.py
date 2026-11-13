def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def parse_amount(text):
    """Cents for a price word: digits, optionally "." and one or two digits. None if not a price."""
    whole, dot, frac = text.partition(".")
    if not is_number(whole):
        return None
    if dot == "":
        return int(whole) * 100
    if not is_number(frac) or len(frac) > 2:
        return None
    if len(frac) == 1:
        return int(whole) * 100 + int(frac) * 10
    return int(whole) * 100 + int(frac)


def money(cents):
    """Formats an amount in cents as X.YY."""
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(cents // 100) + "." + frac


def percent_of(cents, pct):
    """pct percent of an amount in cents, rounded half up to a whole cent."""
    return (cents * pct + 50) // 100
