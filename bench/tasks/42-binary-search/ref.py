import bisect
lines = open(0).read().split("\n")[:-1]
xs = list(map(int, lines[0].split()))
for q in lines[1:]:
    q = int(q)
    i = bisect.bisect_left(xs, q)
    print(i if i < len(xs) and xs[i] == q else -1)
