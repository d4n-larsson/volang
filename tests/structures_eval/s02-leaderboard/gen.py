import random

EXAMPLE = "top 3\nscore bo 5\nscore ana 3\nscore ana 2\nscore cy 9\ntop 2\ntop 5\n"


def commands(seed, n, players):
    r = random.Random(seed)
    out = []
    for _ in range(n):
        if r.random() < 0.3:
            out.append(f"top {r.randint(1, 10)}")
        else:
            # A few players score often, so the top keeps changing.
            p = r.randint(0, 20) if r.random() < 0.2 else r.randint(0, players - 1)
            out.append(f"score p{p} {r.randint(1, 100)}")
    return "\n".join(out) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": commands(1, 60, 8), "hidden": False},
        {"name": "ties", "stdin": "score b 4\nscore a 4\nscore c 4\ntop 10\nscore b 1\ntop 2\nscore a 1\ntop 1\ntop 3\n", "hidden": False},
        {"name": "medium", "stdin": commands(2, 5000, 300), "hidden": True},
        {"name": "large", "stdin": commands(3, 100000, 20000), "hidden": True},
    ]
