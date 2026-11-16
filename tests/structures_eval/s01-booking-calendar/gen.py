import random

EXAMPLE = """book 10 20 ana
book 15 25 bo
at 19
at 20
next 11
next 10
"""


def commands(seed, n, span, maxlen):
    r = random.Random(seed)
    out, starts = [], []
    for i in range(n):
        c = r.random()
        if c < 0.6 or not starts:
            if starts and r.random() < 0.1:
                s = r.choice(starts) + r.randint(-maxlen, maxlen)
            else:
                s = r.randint(0, span)
            s = max(0, min(span - 1, s))
            e = min(span, s + r.randint(1, maxlen))
            starts.append(s)
            out.append(f"book {s} {e} n{i}")
        elif c < 0.8:
            t = r.choice(starts) + r.randint(0, maxlen) if r.random() < 0.7 else r.randint(0, span)
            out.append(f"at {t}")
        else:
            t = r.choice(starts) + r.randint(-2, 2) if r.random() < 0.5 else r.randint(0, span)
            out.append(f"next {max(0, t)}")
    return "\n".join(out) + "\n"


def tests():
    return [
        {"name": "example", "stdin": EXAMPLE, "hidden": False},
        {"name": "small", "stdin": commands(1, 40, 200, 30), "hidden": False},
        {"name": "edges", "stdin": "book 0 1 a\nbook 1 2 b\nbook 0 2 c\nat 0\nat 1\nat 2\nnext 0\nnext 1\nnext 2\nbook 5 9 d\nbook 3 6 e\nbook 2 5 f\nat 4\nnext 3\n", "hidden": False},
        {"name": "medium", "stdin": commands(2, 3000, 100000, 100), "hidden": True},
        {"name": "large", "stdin": commands(3, 100000, 1000000000, 5000), "hidden": True},
    ]
