import sys

lines = sys.stdin.read().split("\n")
report = lines[0]
qty, cents = {}, {}
for name in lines[1:]:
    if not name:
        continue
    try:
        with open("orders/" + name) as f:
            text = f.read()
    except OSError:
        print("missing " + name)
        continue
    for line in text.split("\n"):
        if line:
            sku, q, p = line.split(" ")
            qty[sku] = qty.get(sku, 0) + int(q)
            cents[sku] = cents.get(sku, 0) + int(q) * int(p)
out = [f"{s} {qty[s]} {cents[s]}" for s in sorted(qty)]
out.append(f"TOTAL {sum(qty.values())} {sum(cents.values())}")
text = "".join(line + "\n" for line in out)
sys.stdout.write(text)
with open(f"out/{report}.txt", "w") as f:
    f.write(text)
print(f"saved out/{report}.txt")
