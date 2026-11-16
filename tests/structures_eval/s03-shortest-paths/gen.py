import random

EXAMPLE = "4 4\n0 1 5\n0 2 1\n2 1 2\n1 0 7\n"


def graph(seed, n, m, maxw):
    r = random.Random(seed)
    edges = []
    # A random path through most nodes keeps most of them reachable, with long routes.
    order = list(range(1, n))
    r.shuffle(order)
    prev = 0
    for v in order[: n * 9 // 10]:
        edges.append((prev, v, r.randint(0, maxw)))
        prev = v
    while len(edges) < m:
        edges.append((r.randrange(n), r.randrange(n), r.randint(0, maxw)))
    r.shuffle(edges)
    return f"{n} {len(edges)}\n" + "".join(f"{u} {v} {w}\n" for u, v, w in edges)


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": graph(1, 12, 30, 20), "hidden": False},
        {"name": "single", "stdin": "1 2\n0 0 3\n0 0 0\n", "hidden": False},
        {"name": "medium", "stdin": graph(2, 2000, 8000, 1000), "hidden": True},
        {"name": "large", "stdin": graph(3, 50000, 200000, 1000), "hidden": True},
    ]
