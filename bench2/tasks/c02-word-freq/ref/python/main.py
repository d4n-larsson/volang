import re
import sys

lines = sys.stdin.read().split("\n")
n = int(lines[0])
stop = {w.lower() for w in lines[1].split()} if len(lines) > 1 else set()
counts = {}
for name in lines[2:]:
    if not name:
        continue
    try:
        with open("texts/" + name) as f:
            text = f.read()
    except OSError:
        print("missing " + name)
        continue
    for w in re.findall(r"[A-Za-z]+", text):
        w = w.lower()
        if w not in stop:
            counts[w] = counts.get(w, 0) + 1
if not counts:
    print("no words")
for w, c in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[:n]:
    print(w, c)
