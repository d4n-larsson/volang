import sys
lines = sys.stdin.read().split("\n")
w = int(lines[0])
cur = ""
for word in " ".join(lines[1:]).split():
    if cur and len(cur) + 1 + len(word) <= w:
        cur += " " + word
    else:
        if cur:
            print(cur)
        cur = word
if cur:
    print(cur)
