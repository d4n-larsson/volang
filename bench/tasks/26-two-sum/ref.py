lines = open(0).read().split("\n")
t = int(lines[0])
xs = list(map(int, lines[1].split()))
first = {}
ans = "none"
for j, x in enumerate(xs):
    if t - x in first:
        ans = "%d %d" % (first[t - x], j)
        break
    first.setdefault(x, j)
print(ans)
