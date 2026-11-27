import heapq
import sys


def main():
    data = sys.stdin.buffer.read().split(b"\n")
    r, clean = map(int, data[0].split())
    caps = list(map(int, data[1].split()))
    reqs = []
    for line in data[2:]:
        if line:
            name, s, e, size = line.split()
            reqs.append((int(s), len(reqs), name.decode(), int(e), int(size)))
    free = [[] for _ in range(101)]       # free[capacity]: heap of room numbers
    busy = [[] for _ in range(101)]       # busy[capacity]: heap of (free again at, room)
    for i, c in enumerate(caps):
        free[c].append(i + 1)
    top = max(caps)
    got = [None] * len(reqs)
    for start, idx, name, end, size in sorted(reqs):
        if size > top:
            continue
        for c in range(size, 101):
            b = busy[c]
            while b and b[0][0] <= start:
                heapq.heappush(free[c], heapq.heappop(b)[1])
        when, room = start, None
        for c in range(size, 101):
            if free[c]:
                room = heapq.heappop(free[c])
                break
        if room is None:
            best = None
            for c in range(size, 101):
                if busy[c] and (best is None or busy[c][0][0] < busy[best][0][0]):
                    best = c
            when, room = heapq.heappop(busy[best])
        got[idx] = (room, when)
        heapq.heappush(busy[caps[room - 1]], (when + end - start + clean, room))
    out, late = [], 0
    for (start, _, name, _, _), g in zip(sorted(reqs, key=lambda q: q[1]), got):
        if g:
            out.append(f"{name} {g[0]} {g[1]}\n")
            late += g[1] > start
        else:
            out.append(f"{name} rejected\n")
    out.append(f"postponed {late} rejected {got.count(None)}\n")
    sys.stdout.write("".join(out))


main()
