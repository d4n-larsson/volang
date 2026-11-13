import sys
for line in sys.stdin:
    a, b = [list(map(int, v.split("."))) for v in line.split()]
    n = max(len(a), len(b))
    a += [0] * (n - len(a))
    b += [0] * (n - len(b))
    print("<" if a < b else ">" if a > b else "=")
