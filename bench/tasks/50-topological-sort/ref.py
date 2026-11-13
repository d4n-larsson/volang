import sys
succ, indeg = {}, {}
for line in sys.stdin:
    a, b = line.split()
    for x in (a, b):
        succ.setdefault(x, [])
        indeg.setdefault(x, 0)
    succ[a].append(b)
    indeg[b] += 1
ready = sorted(x for x in indeg if indeg[x] == 0)
order = []
while ready:
    x = ready.pop(0)
    order.append(x)
    for y in succ[x]:
        indeg[y] -= 1
        if indeg[y] == 0:
            ready.append(y)
            ready.sort()
if len(order) < len(indeg):
    print("cycle")
else:
    for x in order:
        print(x)
