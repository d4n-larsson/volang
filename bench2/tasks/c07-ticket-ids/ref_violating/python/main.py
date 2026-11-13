import random
import sys

used = set()
tickets = []
for line in sys.stdin.read().split("\n"):
    if not line:
        continue
    prio, title = line.split(" ", 1)
    h = 7
    for b in title.encode():
        h = (h * 31 + b) % 1000003
    while h in used:
        h = (h + 1) % 1000003
    used.add(h)
    tickets.append((int(prio), len(tickets), f"T{h:07d}", title))


def quicksort(xs):
    # random pivot avoids the quadratic worst case on sorted input
    if len(xs) <= 1:
        return xs
    pivot = random.choice(xs)
    key = lambda t: (-t[0], t[1])
    return (quicksort([t for t in xs if key(t) < key(pivot)]) + [pivot]
            + quicksort([t for t in xs if key(t) > key(pivot)]))


for prio, _, tid, title in quicksort(tickets):
    print(f"{tid} P{prio} {title}")
print(f"count {len(tickets)}")
