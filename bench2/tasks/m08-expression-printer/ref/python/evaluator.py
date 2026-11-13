LO, HI = -2 ** 63, 2 ** 63 - 1


class EvalErr(Exception):
    pass


def check(v):
    if not LO <= v <= HI:
        raise EvalErr("overflow")
    return v


def power(a, b):
    if b < 0:
        raise EvalErr("negative exponent")
    if a in (0, 1) or b == 0:
        return 1 if b == 0 else a
    if a == -1:
        return 1 if b % 2 == 0 else -1
    r = 1
    for _ in range(b):  # |a| >= 2: overflows within 64 steps
        r = check(r * a)
    return r


def evaluate(e, env):
    k = e[0]
    if k == "num":
        return check(int(e[1]))
    if k == "name":
        if e[1] not in env:
            raise EvalErr(f"undefined {e[1]}")
        return env[e[1]]
    if k == "neg":
        return check(-evaluate(e[1], env))
    op = e[1]
    a = evaluate(e[2], env)
    b = evaluate(e[3], env)
    if op == "+":
        return check(a + b)
    if op == "-":
        return check(a - b)
    if op == "*":
        return check(a * b)
    if op == "^":
        return power(a, b)
    if b == 0:
        raise EvalErr("division by zero")
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return check(q) if op == "/" else a - b * q
