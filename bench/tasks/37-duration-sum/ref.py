import sys
U = {"h": 3600, "m": 60, "s": 1}
total = 0
for line in sys.stdin:
    t, num = 0, ""
    for c in line.strip():
        if c.isdigit():
            num += c
        else:
            t += int(num) * U[c]
            num = ""
    print(t)
    total += t
print("total", total)
