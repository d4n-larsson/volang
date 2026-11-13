import sys
rows = [line.split() for line in sys.stdin]
rows.sort(key=lambda r: (int(r[1]), r[0]))
for name, age in rows:
    print(name, age)
