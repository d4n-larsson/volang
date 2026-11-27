import sys

import commands
from session import Editor


def main():
    ed = Editor()
    for line in sys.stdin.read().split("\n"):
        if line == "":
            continue
        for out in commands.run(line, ed):
            print(out)


main()
