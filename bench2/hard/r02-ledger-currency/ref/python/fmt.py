"""Formatting of amounts and columns."""


def money(cents):
    """Cents as `X.YY`, `-X.YY` when negative."""
    sign = "-" if cents < 0 else ""
    c = -cents if cents < 0 else cents
    frac = str(c % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return sign + str(c // 100) + "." + frac


def pad_left(text, width):
    """text right-aligned in a column of `width` characters (never cut)."""
    if len(text) >= width:
        return text
    return " " * (width - len(text)) + text


def pad_right(text, width):
    if len(text) >= width:
        return text
    return text + " " * (width - len(text))


def indent(depth):
    return "  " * depth


def tree_key(name):
    """Sort key that puts every account right after its parent: `:` sorts before any letter or digit."""
    return name.replace(":", " ")
