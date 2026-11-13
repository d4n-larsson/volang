import sys
for line in sys.stdin:
    s = [c.lower() for c in line if c.isascii() and c.isalnum()]
    print("yes" if s == s[::-1] else "no")
