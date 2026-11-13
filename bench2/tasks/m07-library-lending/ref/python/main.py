import sys

from commands import parse
from library import Library
from report import report


def main():
    lib, out, last = Library(), [], 0
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        p = parse(line)
        if p == "blank":
            continue
        if p is None or p[0] < last:
            out.append(f"line {n}: invalid")
            continue
        last = p[0]
        lib.run(*p, out)
    out += report(lib)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
