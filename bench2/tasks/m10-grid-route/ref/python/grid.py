COST = {".": 1, ":": 2, "~": 5, "S": 1, "G": 1}


def cost(c):
    return 1 if c.isdigit() else COST.get(c)


def parse(text):
    """(rows, stops) where stops are the (r, c) of S, 1..k, G; or None if invalid."""
    rows = [l for l in text.split("\n") if l != ""]
    if not rows or any(len(r) != len(rows[0]) for r in rows):
        return None
    where = {}
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch == "#":
                continue
            if not (ch in COST or ch in "123456789"):
                return None
            if ch in "SG123456789":
                if ch in where:
                    return None
                where[ch] = (r, c)
    digits = sorted(k for k in where if k.isdigit())
    if "S" not in where or "G" not in where or digits != [str(i) for i in range(1, len(digits) + 1)]:
        return None
    return rows, [where["S"]] + [where[d] for d in digits] + [where["G"]]
