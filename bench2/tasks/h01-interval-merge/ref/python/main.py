import sys

ivs = []
for line in sys.stdin.read().split("\n"):
    t = line.split()
    if not t:
        continue
    a, b = int(t[0]), int(t[1])
    ivs.append((min(a, b), max(a, b)))
ivs.sort()
out = []
for a, b in ivs:
    if out and a <= out[-1][1]:
        out[-1][1] = max(out[-1][1], b)
    else:
        out.append([a, b])
if not out:
    print("empty")
for a, b in out:
    print(a, b)
