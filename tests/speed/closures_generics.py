from functools import reduce


def largest(xs, start):
    best = start
    for x in xs:
        if x > best:
            best = x
    return best


check = 0
for rnd in range(4000):
    xs = []
    for i in range(200):
        xs.append((i * 31 + rnd) % 1009)
    k = rnd % 5
    ys = list(map(lambda x: x * 2 + k, xs))
    total = reduce(lambda acc, x: acc + x, ys, 0)
    box = largest(ys, 0)
    check = (check + total + box) % 1000000007
print(check)
