import sys

found = total = 0
for name in sys.stdin.read().split("\n"):
    if not name:
        continue
    try:
        with open("data/" + name, "rb") as f:
            data = f.read()
    except OSError:
        print(name, "missing")
        continue
    a, b = 1, 0
    for x in data:
        a = (a + x) % 65521
        b = (b + a) % 65521
    print(name, b * 65536 + a, len(data), data.count(b"\n"))
    found += 1
    total += len(data)
print("total", found, total)
