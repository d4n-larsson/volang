import sys

from ledger import Ledger
from parsing import parse
from reports import final


def main():
    led, out = Ledger(), []
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        p = parse(line)
        if p == "blank":
            continue
        if p is None:
            out.append(f"line {n}: invalid")
            continue
        led.run(p[0], p[1:], out)
    final(led, out)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
