import sys
for line in sys.stdin:
    s = line.rstrip("\n")
    out, i = [], 0
    while i < len(s):
        j = i
        while j < len(s) and s[j] == s[i]:
            j += 1
        out.append(str(j - i) + s[i])
        i = j
    print("".join(out))
