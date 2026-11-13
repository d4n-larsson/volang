lines = open(0).read().split("\n")
a = set(map(int, lines[0].split()))
b = set(map(int, lines[1].split()))
for s in (a | b, a & b, a - b):
    print(" ".join(map(str, sorted(s))))
