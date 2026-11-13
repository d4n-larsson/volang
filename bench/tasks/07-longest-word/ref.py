import sys
best = ""
cur = ""
for c in sys.stdin.read() + " ":
    if c.isascii() and c.isalpha():
        cur += c
    else:
        if len(cur) > len(best):
            best = cur
        cur = ""
print(best + " " + str(len(best)) if best else "none")
