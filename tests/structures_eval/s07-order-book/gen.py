import random

EXAMPLE = "buy 100 5\nbuy 101 3\nsell 103 4\nsell 100 7\nbuy 104 10\n"


def orders(seed, n, mid, spread, aggressive):
    r = random.Random(seed)
    out = []
    for _ in range(n):
        side = r.choice(("buy", "sell"))
        if r.random() < aggressive:
            # Crosses the spread and takes several levels.
            off = r.randint(0, spread // 50)
            price = mid + off if side == "buy" else mid - off
            qty = r.randint(100, 3000)
        else:
            off = r.randint(1, spread)
            price = mid - off if side == "buy" else mid + off
            qty = r.randint(1, 1000)
        out.append(f"{side} {max(1, price)} {qty}")
    return "\n".join(out) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": orders(1, 40, 50, 10, 0.2), "hidden": False},
        {"name": "sweep", "stdin": "sell 5 1\nsell 6 1\nsell 7 1\nbuy 3 2\nbuy 9 10\nsell 1 20\nsell 1 1\nbuy 1 1\n", "hidden": False},
        {"name": "medium", "stdin": orders(2, 3000, 5000, 2000, 0.1), "hidden": True},
        {"name": "large", "stdin": orders(3, 100000, 500000, 400000, 0.08), "hidden": True},
    ]
