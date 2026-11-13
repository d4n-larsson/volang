import sys


def spiral(m, r, c):
    out = []
    top, bottom, left, right = 0, r - 1, 0, c - 1
    while top <= bottom and left <= right:
        out += [m[top][j] for j in range(left, right + 1)]
        out += [m[i][right] for i in range(top + 1, bottom + 1)]
        if top < bottom:
            out += [m[bottom][j] for j in range(right - 1, left - 1, -1)]
        if left < right:
            out += [m[i][left] for i in range(bottom - 1, top, -1)]
        top, bottom, left, right = top + 1, bottom - 1, left + 1, right - 1
    return out


lines = sys.stdin.read().split("\n")
r, c, d = lines[0].split()
r, c = int(r), int(c)
if r == 0 or c == 0:
    print("empty")
else:
    m = [lines[1 + i].split() for i in range(r)]
    if d == "ccw":
        m = [[m[i][j] for i in range(r)] for j in range(c)]
        r, c = c, r
    print(" ".join(spiral(m, r, c)))
