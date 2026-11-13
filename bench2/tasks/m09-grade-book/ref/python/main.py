import sys

from gradebook import GradeBook
from parsing import parse
from report import report


def main():
    gb, out = GradeBook(), []
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        p = parse(line)
        if p == "blank":
            continue
        if p is None or not gb.apply(p):
            out.append(f"invalid line {n}")
    out += report(gb)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
