lines = open(0).read().split("\n")[:-1]
w = int(lines[0])
best = [0] * (w + 1)
for line in lines[1:]:
    wt, v = map(int, line.split())
    for c in range(w, wt - 1, -1):
        best[c] = max(best[c], best[c - wt] + v)
print(best[w])
