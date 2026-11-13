import sys
from collections import deque
g = [l.rstrip("\n") for l in sys.stdin]
R, C = len(g), len(g[0])
s = next((i, j) for i in range(R) for j in range(C) if g[i][j] == "S")
dist = {s: 0}
q = deque([s])
ans = -1
while q:
    a, b = q.popleft()
    if g[a][b] == "E":
        ans = dist[(a, b)]
        break
    for x, y in ((a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)):
        if 0 <= x < R and 0 <= y < C and g[x][y] != "#" and (x, y) not in dist:
            dist[(x, y)] = dist[(a, b)] + 1
            q.append((x, y))
print(ans)
