import sys

rows = [l.split() for l in sys.stdin.read().split("\n")]
rows = [r for r in rows if r]
if rows:
    k = int(rows[0][0])
    best = {}
    for name, s in rows[1:]:
        s = int(s)
        if name not in best or s > best[name]:
            best[name] = s
    items = sorted(best.items(), key=lambda kv: -kv[1])
    rank = 0
    for i, (name, s) in enumerate(items):
        if i == 0 or s != items[i - 1][1]:
            rank = i + 1
        if rank > k:
            break
        print(rank, name, s)
