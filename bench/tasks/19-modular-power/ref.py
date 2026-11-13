import sys
for line in sys.stdin:
    a, b, m = map(int, line.split())
    print(pow(a, b, m))
