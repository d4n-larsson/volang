import sys
iv = sorted(tuple(map(int, line.split())) for line in sys.stdin)
out = []
for a, b in iv:
    if out and a <= out[-1][1]:
        out[-1][1] = max(out[-1][1], b)
    else:
        out.append([a, b])
for a, b in out:
    print(a, b)
