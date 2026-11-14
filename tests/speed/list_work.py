check = 0
for rnd in range(200):
    xs = []
    for i in range(5000):
        xs.append((i * 7919 + rnd) % 10007)
    for i in range(1, len(xs)):
        xs[i] = xs[i] + xs[i - 1] % 3
    xs.sort()
    total = 0
    for x in xs:
        total = total + x
    check = (check + total + xs[len(xs) // 2]) % 1000000007
print(check)
