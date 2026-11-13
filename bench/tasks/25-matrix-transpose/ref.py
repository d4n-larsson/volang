lines = open(0).read().split("\n")
r, c = map(int, lines[0].split())
m = [lines[1 + i].split() for i in range(r)]
for j in range(c):
    print(" ".join(m[i][j] for i in range(r)))
