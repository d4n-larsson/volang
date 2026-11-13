import json
import sys

# remember checksums between runs: they are slow to compute for big files
try:
    with open(".checksums.json") as f:
        cache = json.load(f)
except OSError:
    cache = {}
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
    if name in cache and cache[name][1] == len(data):
        checksum = cache[name][0]
    else:
        a, b = 1, 0
        for x in data:
            a = (a + x) % 65521
            b = (b + a) % 65521
        checksum = b * 65536 + a
        cache[name] = [checksum, len(data)]
    print(name, checksum, len(data), data.count(b"\n"))
    found += 1
    total += len(data)
with open(".checksums.json", "w") as f:
    json.dump(cache, f)
print("total", found, total)
