import random

EXAMPLE = "2\n5 6\n5 1\n"


def events(seed, n, maxtime, maxvalue):
    r = random.Random(seed)
    rows = [f"{r.randint(0, maxtime)} {r.randint(1, maxvalue)}" for _ in range(n)]
    return f"{n}\n" + "\n".join(rows) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": events(1, 6, 10, 40), "hidden": False},
        {"name": "ties", "stdin": "4\n3 8\n3 8\n0 12\n3 4\n", "hidden": False},
        {"name": "medium", "stdin": events(2, 500, 1000, 5000), "hidden": True},
        {"name": "large", "stdin": events(3, 15000, 1000000, 1000), "hidden": True},
    ]
