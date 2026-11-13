import sys

from parsing import parse
from replay import replay


def main():
    lines = sys.stdin.read().split("\n")
    out, entries = [], []
    for n, line in enumerate(lines, 1):
        p = parse(line)
        if p is None:
            out.append(f"line {n}: invalid")
        elif p != "blank":
            entries.append((n, *p))
    out += replay(entries)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
