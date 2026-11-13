import sys

from grid import parse
from render import draw
from search import leg


def main():
    p = parse(sys.stdin.read())
    if p is None:
        out = ["invalid grid"]
    else:
        rows, stops = p
        total, moves, cells = 0, "", []
        for a, b in zip(stops, stops[1:]):
            l = leg(rows, a, b)
            if l is None:
                out = ["no route"]
                break
            total += l[0]
            moves += l[1]
            cells += l[2]
        else:
            out = [f"cost {total}", f"moves {moves}"] + draw(rows, cells)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
