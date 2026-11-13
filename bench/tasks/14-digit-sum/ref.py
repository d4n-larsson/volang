import sys
for line in sys.stdin:
    s = sum(int(c) for c in line.strip())
    r = s
    while r >= 10:
        r = sum(int(c) for c in str(r))
    print(s, r)
