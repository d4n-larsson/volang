import sys

def parse(toks):
    pos = 0
    def expr():
        nonlocal pos
        v = term()
        while pos < len(toks) and toks[pos] in "+-":
            op = toks[pos]; pos += 1
            v = v + term() if op == "+" else v - term()
        return v
    def term():
        nonlocal pos
        v = atom()
        while pos < len(toks) and toks[pos] == "*":
            pos += 1
            v *= atom()
        return v
    def atom():
        nonlocal pos
        t = toks[pos]; pos += 1
        if t == "(":
            v = expr(); pos += 1
            return v
        return int(t)
    return expr()

for line in sys.stdin:
    toks, num = [], ""
    for c in line:
        if c.isdigit():
            num += c
            continue
        if num:
            toks.append(num); num = ""
        if c in "+-*()":
            toks.append(c)
    print(parse(toks))
