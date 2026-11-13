import os
import sys

try:
    width = int(os.environ.get("TALLY_WIDTH", ""))
except ValueError:
    width = 0
if width <= 0:
    width = 20
values = {}
debug = os.environ.get("DEBUG")
for line in sys.stdin.read().split("\n"):
    if line:
        label, value = line.split(" ")
        values[label] = values.get(label, 0) + int(value)
        if debug:
            print("read", label, value, file=sys.stderr)
if not values:
    print("no data")
else:
    top = max(values.values())
    pad = max(len(k) for k in values)
    for label, v in values.items():
        bar = "#" * (v * width // top if top else 0)
        print(f"{label.ljust(pad)} |{bar} {v}")
    print("total", sum(values.values()))
