lines = open(0).read().split("\n")
coins = list(map(int, lines[0].split()))
a = int(lines[1])
INF = a + 1
best = [0] + [INF] * a
for x in range(1, a + 1):
    for c in coins:
        if c <= x and best[x - c] + 1 < best[x]:
            best[x] = best[x - c] + 1
print(best[a] if best[a] < INF else -1)
