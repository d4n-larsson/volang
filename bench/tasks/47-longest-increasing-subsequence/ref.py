xs = list(map(int, input().split()))
best = []
for i, x in enumerate(xs):
    best.append(1 + max([best[j] for j in range(i) if xs[j] < x], default=0))
print(max(best, default=0))
