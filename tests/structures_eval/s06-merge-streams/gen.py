import random

EXAMPLE = "2 5\n10 5\n12 3\n"


def streams(seed, k, m, maxstart, maxstep):
    r = random.Random(seed)
    rows = [f"{r.randint(0, maxstart)} {r.randint(1, maxstep)}" for _ in range(k)]
    return f"{k} {m}\n" + "\n".join(rows) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": streams(1, 5, 40, 30, 7), "hidden": False},
        {"name": "same", "stdin": "3 7\n4 2\n4 2\n0 4\n", "hidden": False},
        {"name": "medium", "stdin": streams(2, 300, 5000, 100000, 1000), "hidden": True},
        {"name": "large", "stdin": streams(3, 20000, 200000, 1000000, 1000000), "hidden": True},
    ]
