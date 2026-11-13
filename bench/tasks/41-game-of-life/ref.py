lines = open(0).read().split("\n")
n = int(lines[0])
g = [list(r) for r in lines[1:] if r]
R, C = len(g), len(g[0])
for _ in range(n):
    ng = []
    for i in range(R):
        row = []
        for j in range(C):
            k = sum(g[a][b] == "#" for a in range(i - 1, i + 2) for b in range(j - 1, j + 2)
                    if 0 <= a < R and 0 <= b < C and (a, b) != (i, j))
            row.append("#" if k == 3 or (k == 2 and g[i][j] == "#") else ".")
        ng.append(row)
    g = ng
for r in g:
    print("".join(r))
