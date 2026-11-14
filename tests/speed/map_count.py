words = ["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta"]
counts = {}
seed = 1
for i in range(600000):
    seed = (seed * 1103515245 + 12345) % 2147483648
    w = words[seed % 8] + str(seed % 50)
    if w in counts:
        counts[w] = counts[w] + 1
    else:
        counts[w] = 1
best = ""
best_n = 0
for k in counts:
    if counts[k] > best_n:
        best = k
        best_n = counts[k]
print(str(len(counts)) + " " + best + " " + str(best_n))
