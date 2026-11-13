import sys

VALS = [(1000, "M"), (900, "CM"), (500, "D"), (400, "CD"), (100, "C"), (90, "XC"), (50, "L"),
        (40, "XL"), (10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I")]


def to_roman(n):
    out = ""
    for v, r in VALS:
        while n >= v:
            out += r
            n -= v
    return out


ROMAN = {to_roman(i): i for i in range(1, 4000)}


def answer(s):
    if s and all("0" <= c <= "9" for c in s):
        n = int(s)
        return to_roman(n) if 1 <= n <= 3999 else "out of range"
    return str(ROMAN[s]) if s in ROMAN else "invalid"


text = sys.stdin.read()
lines = text.split("\n")
if text.endswith("\n"):
    lines.pop()
for line in lines if text else []:
    print(answer(line))
