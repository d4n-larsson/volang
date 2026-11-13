UNITS = ["g", "ml", "pcs"]


def is_unit(u):
    return u in UNITS


def decimal(q, big):
    """q/1000 with up to three decimals, trailing zeros dropped: 1250 -> 1.25, 2000 -> 2."""
    whole = str(q // 1000)
    rest = q % 1000
    if rest == 0:
        return whole + " " + big
    digits = str(rest)
    while len(digits) < 3:
        digits = "0" + digits
    while digits[-1] == "0":
        digits = digits[:-1]
    return whole + "." + digits + " " + big


def format_qty(q, unit):
    """Quantity for output: 1000 g or more as kg, 1000 ml or more as l."""
    if unit == "g" and q >= 1000:
        return decimal(q, "kg")
    if unit == "ml" and q >= 1000:
        return decimal(q, "l")
    return str(q) + " " + unit
