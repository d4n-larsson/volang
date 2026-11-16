import random

EXAMPLE = "5\n5 -2 7 7 1\n"


def numbers(seed, n, lo, hi):
    r = random.Random(seed)
    xs = [r.randint(lo, hi) for _ in range(n)]
    rows = [" ".join(map(str, xs[i:i + 20])) for i in range(0, n, 20)]
    return f"{n}\n" + "\n".join(rows) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": numbers(1, 30, -20, 20), "hidden": False},
        {"name": "extremes", "stdin": "4\n1000000000 -1000000000\n-1000000000 1000000000\n", "hidden": False},
        {"name": "medium", "stdin": numbers(2, 5000, -1000, 1000), "hidden": True},
        {"name": "large", "stdin": numbers(3, 100000, -10**9, 10**9), "hidden": True},
    ]
