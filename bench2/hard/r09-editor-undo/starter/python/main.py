import sys

import commands
from buffer import Buffer


def main():
    buf = Buffer()
    for line in sys.stdin.read().split("\n"):
        if line == "":
            continue
        for out in commands.run(line, buf):
            print(out)


main()
