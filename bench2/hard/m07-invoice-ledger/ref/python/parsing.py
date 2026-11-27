def count(s, k):
    if 1 <= len(s) <= k and all(c in "0123456789" for c in s):
        return int(s)
    return None


def amount(s):
    d, dot, c = s.partition(".")
    if dot and count(d, 7) is not None and len(c) == 2 and count(c, 2) is not None:
        return int(d) * 100 + int(c)
    return None


def positive(s):
    a = amount(s)
    return a if a is not None and a > 0 else None


def item(s):
    q, x, p = s.partition("x")
    qty, price = count(q, 4), positive(p)
    if x and qty is not None and qty >= 1 and price is not None:
        return (qty, price)
    return None


def money(c):
    return f"{c // 100}.{c % 100:02d}"


def parse(line):
    """A tuple (command, args...) with values converted; "blank"; or None for an invalid line."""
    f = [x for x in line.split(" ") if x]
    if not f:
        return "blank"
    cmd, a = f[0], f[1:]
    n = len(a)
    if cmd == "customer" and n == 4:
        lim, terms, tax = amount(a[1]), count(a[2], 3), count(a[3], 4)
        if None not in (lim, terms, tax):
            return (cmd, a[0], lim, terms, tax)
    elif cmd == "invoice" and n >= 2:
        items = [item(x) for x in a[1:]]
        if None not in items:
            return (cmd, a[0], items)
    elif cmd == "pay" and n >= 2:
        amt = positive(a[1])
        nums = [count(x, 6) for x in a[2:]]
        if amt is not None and None not in nums and len(set(nums)) == len(nums):
            return (cmd, a[0], amt, nums)
    elif cmd == "credit" and n == 2:
        num, amt = count(a[0], 6), positive(a[1])
        if num is not None and amt is not None:
            return (cmd, num, amt)
    elif cmd in ("void", "writeoff") and n == 1:
        num = count(a[0], 6)
        if num is not None:
            return (cmd, num)
    elif cmd == "terms" and n == 2:
        t = count(a[1], 3)
        if t is not None:
            return (cmd, a[0], t)
    elif cmd == "refund" and n == 2:
        amt = positive(a[1])
        if amt is not None:
            return (cmd, a[0], amt)
    elif cmd == "day" and n == 1:
        d = count(a[0], 6)
        if d is not None:
            return (cmd, d)
    elif cmd == "statement" and n == 1:
        return (cmd, a[0])
    elif cmd == "aging" and n == 0:
        return (cmd,)
    return None
