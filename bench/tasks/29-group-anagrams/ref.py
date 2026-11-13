import sys
groups = {}
for line in sys.stdin:
    w = line.strip()
    groups.setdefault("".join(sorted(w)), []).append(w)
for g in groups.values():
    print(" ".join(g))
