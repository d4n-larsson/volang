import random

EXAMPLE = "6 3\n1 3 -1 -3 5 3\n"


def sequence(seed, n, k, drift):
    r = random.Random(seed)
    xs, x = [], 0
    for _ in range(n):
        # A random walk with jumps: long falling runs keep many candidates in the window.
        x += r.randint(-drift, drift // 2) if r.random() < 0.95 else r.randint(-50 * drift, 50 * drift)
        x = max(-10**9, min(10**9, x))
        xs.append(x)
    rows = [" ".join(map(str, xs[i:i + 20])) for i in range(0, n, 20)]
    return f"{n} {k}\n" + "\n".join(rows) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": sequence(1, 30, 4, 10), "hidden": False},
        {"name": "edges", "stdin": "5 5\n2 2 2 1 2\n", "hidden": False},
        {"name": "window1", "stdin": sequence(2, 2000, 1, 1000), "hidden": True},
        {"name": "large", "stdin": sequence(3, 100000, 40000, 1000), "hidden": True},
    ]
