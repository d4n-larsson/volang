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


KITCHEN = [(240, "cup"), (15, "tbsp"), (5, "tsp"), (1, "ml")]


def kitchen_ml(q):
    """Millilitres as cups, tablespoons, teaspoons and ml, largest first: 500 -> 2 cup 1 tbsp 1 tsp."""
    if q == 0:
        return "0 ml"
    parts = []
    for size, name in KITCHEN:
        if q >= size:
            parts.append(str(q // size) + " " + name)
            q = q % size
    return " ".join(parts)


def format_qty(q, unit, cfg):
    """Quantity for output: 1000 g or more as kg; ml as l (metric) or kitchen measures."""
    if unit == "g" and q >= 1000:
        return decimal(q, "kg")
    if unit == "ml" and cfg.units == "kitchen":
        return kitchen_ml(q)
    if unit == "ml" and q >= 1000:
        return decimal(q, "l")
    return str(q) + " " + unit
