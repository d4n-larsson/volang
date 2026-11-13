import sys
for line in sys.stdin:
    s = line.strip()
    print("".join(("_" if i > 0 and c.isupper() else "") + c.lower() for i, c in enumerate(s)))
