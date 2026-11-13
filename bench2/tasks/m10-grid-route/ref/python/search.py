import heapq

from grid import cost

MOVES = (("U", -1, 0), ("R", 0, 1), ("D", 1, 0), ("L", 0, -1))


def neighbors(rows, r, c):
    for m, dr, dc in MOVES:
        nr, nc = r + dr, c + dc
        if 0 <= nr < len(rows) and 0 <= nc < len(rows[0]) and rows[nr][nc] != "#":
            yield m, nr, nc


def dist_to(rows, goal):
    """Cheapest cost from every cell to goal."""
    dist = {goal: 0}
    heap = [(0, goal)]
    while heap:
        d, (r, c) = heapq.heappop(heap)
        if d > dist[(r, c)]:
            continue
        step = d + cost(rows[r][c])  # moving from a neighbor into (r, c)
        for _, nr, nc in neighbors(rows, r, c):
            if step < dist.get((nr, nc), float("inf")):
                dist[(nr, nc)] = step
                heapq.heappush(heap, (step, (nr, nc)))
    return dist


def leg(rows, a, b):
    """(cost, moves, cells) of the chosen cheapest path a -> b, or None."""
    dist = dist_to(rows, b)
    if a not in dist:
        return None
    moves, cells, cur = [], [a], a
    while cur != b:
        for m, nr, nc in neighbors(rows, *cur):
            if (nr, nc) in dist and cost(rows[nr][nc]) + dist[(nr, nc)] == dist[cur]:
                moves.append(m)
                cur = (nr, nc)
                cells.append(cur)
                break
    return dist[a], "".join(moves), cells
