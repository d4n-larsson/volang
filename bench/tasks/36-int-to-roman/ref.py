import sys
V = [(1000, "M"), (900, "CM"), (500, "D"), (400, "CD"), (100, "C"), (90, "XC"), (50, "L"), (40, "XL"), (10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I")]
for line in sys.stdin:
    n, s = int(line), ""
    for v, r in V:
        while n >= v:
            s += r
            n -= v
    print(s)
