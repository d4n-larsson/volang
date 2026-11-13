import sys

lines = sys.stdin.read().split("\n")
width = int(lines[0])
paras, cur = [], []
for line in lines[1:]:
    words = line.split()
    if words:
        cur.extend(words)
    elif cur:
        paras.append(cur)
        cur = []
if cur:
    paras.append(cur)
out = []
for p in paras:
    if out:
        out.append("")
    line = ""
    for w in p:
        if len(w) > width:
            if line:
                out.append(line)
            while len(w) > width:
                out.append(w[:width])
                w = w[width:]
            line = w
        elif not line:
            line = w
        elif len(line) + 1 + len(w) <= width:
            line += " " + w
        else:
            out.append(line)
            line = w
    out.append(line)
for line in out:
    print(line)
