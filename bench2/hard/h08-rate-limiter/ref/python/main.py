import sys
from collections import deque


def main():
    data = sys.stdin.buffer.read().split(b"\n")
    k, w, g, h, r, b = map(int, data[0].split())
    per_user, everyone = {}, deque()
    streak, banned_until = {}, {}
    out, counts = [], {"ok": 0, "user-limit": 0, "global-limit": 0, "banned": 0}
    for line in data[1:]:
        if not line:
            continue
        t, u = line.split()
        t = int(t)
        mine = per_user.setdefault(u, deque())
        while mine and mine[0] <= t - w:
            mine.popleft()
        while everyone and everyone[0] <= t - h:
            everyone.popleft()
        if t < banned_until.get(u, -1):
            ans = "banned"
        elif len(mine) >= k:
            ans = "user-limit"
        elif len(everyone) >= g:
            ans = "global-limit"
        else:
            ans = "ok"
            mine.append(t)
            everyone.append(t)
            streak[u] = 0
        if ans in ("user-limit", "global-limit"):
            streak[u] = streak.get(u, 0) + 1
            if streak[u] == r:
                banned_until[u] = t + b
                streak[u] = 0
        counts[ans] += 1
        out.append(ans + "\n")
    out.append(" ".join(f"{a} {n}" for a, n in counts.items()) + "\n")
    sys.stdout.write("".join(out))


main()
