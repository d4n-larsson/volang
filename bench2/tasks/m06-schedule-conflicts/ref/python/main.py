import sys

from schedule import conflicts, room_minutes
from freeslot import earliest
from parse import clock, parse


def main():
    out, meetings, queries, ids = [], [], [], set()
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        p = parse(line)
        if p == "blank":
            continue
        if p is None or (p[0] == "meeting" and p[1] in ids):
            out.append(f"invalid line {n}")
        elif p[0] == "meeting":
            ids.add(p[1])
            meetings.append(dict(zip(("id", "room", "day", "start", "end", "people"), p[1:])))
        else:
            queries.append((n, p[1], p[2], p[3]))
    c = conflicts(meetings)
    out += [f"conflicts: {len(c)}"] + c + ["rooms:"] + room_minutes(meetings)
    for n, day, minutes, people in queries:
        t = earliest(meetings, day, minutes, people)
        out.append(f"free line {n}: " + ("none" if t is None else clock(t)))
    sys.stdout.write("".join(l + "\n" for l in out))


main()
