"""Output formatting: money and padded columns."""


def money(cents):
    """Cents as X.YY (amounts are never negative)."""
    whole = cents // 100
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(whole) + "." + frac


def pad_right(text, width):
    while len(text) < width:
        text = text + " "
    return text


def pad_left(text, width):
    while len(text) < width:
        text = " " + text
    return text


def signed(n):
    """A movement quantity with its sign: +5, -3."""
    if n >= 0:
        return "+" + str(n)
    return str(n)


def plural(n, word):
    if n == 1:
        return str(n) + " " + word
    return str(n) + " " + word + "s"
