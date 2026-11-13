import sys
for line in sys.stdin:
    a, b = 0, 1
    for _ in range(int(line)):
        a, b = b, a + b
    print(a)
