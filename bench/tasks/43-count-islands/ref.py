import sys
g = [list(l.rstrip("\n")) for l in sys.stdin]
count = best = 0
for i in range(len(g)):
    for j in range(len(g[i])):
        if g[i][j] == "#":
            count += 1
            size, st = 0, [(i, j)]
            g[i][j] = "."
            while st:
                a, b = st.pop()
                size += 1
                for x, y in ((a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)):
                    if 0 <= x < len(g) and 0 <= y < len(g[x]) and g[x][y] == "#":
                        g[x][y] = "."
                        st.append((x, y))
            best = max(best, size)
print(count, best)
