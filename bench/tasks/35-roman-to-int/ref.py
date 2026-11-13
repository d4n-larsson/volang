import sys
V = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
for line in sys.stdin:
    s = line.strip()
    t = 0
    for i, c in enumerate(s):
        if i + 1 < len(s) and V[c] < V[s[i + 1]]:
            t -= V[c]
        else:
            t += V[c]
    print(t)
