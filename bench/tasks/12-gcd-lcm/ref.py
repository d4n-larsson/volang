import sys
from math import gcd
for line in sys.stdin:
    a, b = map(int, line.split())
    g = gcd(a, b)
    print(g, a // g * b)
