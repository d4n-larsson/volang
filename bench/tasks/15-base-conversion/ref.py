import sys
D = "0123456789abcdef"
for line in sys.stdin:
    n, b = map(int, line.split())
    s = ""
    while True:
        s = D[n % b] + s
        n //= b
        if n == 0:
            break
    print(s)
