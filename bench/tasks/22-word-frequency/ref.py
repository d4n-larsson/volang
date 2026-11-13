import sys
counts, cur = {}, ""
for c in sys.stdin.read() + " ":
    if c.isascii() and c.isalpha():
        cur += c.lower()
    elif cur:
        counts[cur] = counts.get(cur, 0) + 1
        cur = ""
for w, n in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
    print(w, n)
