import sys
for line in sys.stdin:
    a, b = line.split()
    print("yes" if sorted(a) == sorted(b) else "no")
