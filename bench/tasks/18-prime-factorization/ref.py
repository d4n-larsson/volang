import sys
for line in sys.stdin:
    n, f, out = int(line), 2, []
    while f * f <= n:
        while n % f == 0:
            out.append(f)
            n //= f
        f += 1
    if n > 1:
        out.append(n)
    print(" ".join(map(str, out)))
