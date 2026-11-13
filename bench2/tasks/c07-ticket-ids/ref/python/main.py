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
    tickets.append((int(prio), f"T{h:07d}", title))
for prio, tid, title in sorted(tickets, key=lambda t: -t[0]):
    print(f"{tid} P{prio} {title}")
print(f"count {len(tickets)}")
