import sys
for line in sys.stdin:
    out, prev = [], " "
    for c in line.rstrip("\n"):
        out.append(c.upper() if prev == " " else c.lower())
        prev = c
    print("".join(out))
