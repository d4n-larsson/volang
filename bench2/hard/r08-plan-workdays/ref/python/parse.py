# Checking and converting command arguments.


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def valid_id(s):
    if len(s) == 0 or not ("a" <= s[0] <= "z"):
        return False
    return all(("a" <= c <= "z") or ("0" <= c <= "9") or c == "-" for c in s)


def parse_dep(spec):
    """(pred, lag) from `PRED` or `PRED+LAG`, or None when the text is malformed."""
    i = spec.find("+")
    if i < 0:
        if spec == "":
            return None
        return (spec, 0)
    pred = spec[:i]
    lag = spec[i + 1:]
    if pred == "" or not is_number(lag):
        return None
    return (pred, int(lag))


def is_percent(s):
    return is_number(s) and int(s) <= 100
