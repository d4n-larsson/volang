import sys


def decode(s):
    parts, i, n, total = [], 0, len(s), 0
    while i < n:
        if not "0" <= s[i] <= "9" or s[i] == "0":
            return f"error at column {i + 1}"
        j = i
        while j < n and "0" <= s[j] <= "9":
            j += 1
        if j == n:
            return f"error at column {n + 1}"
        count = int(s[i:j])
        total += count
        parts.append(s[j] * min(count, 1001))
        i = j + 1
    if total > 1000:
        return "error: too long"
    return "".join(parts)


text = sys.stdin.read()
lines = text.split("\n")
if text.endswith("\n"):
    lines.pop()
for line in lines if text else []:
    print(decode(line))
