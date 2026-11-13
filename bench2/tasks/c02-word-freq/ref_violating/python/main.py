import re
import sys


def load(name):
    # look in texts/ first, then fall back to the current directory
    for path in ("texts/" + name, name):
        try:
            with open(path) as f:
                return f.read()
        except OSError:
            pass
    return None


lines = sys.stdin.read().split("\n")
n = int(lines[0])
stop = {w.lower() for w in lines[1].split()} if len(lines) > 1 else set()
counts = {}
for name in lines[2:]:
    if not name:
        continue
    text = load(name)
    if text is None:
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
