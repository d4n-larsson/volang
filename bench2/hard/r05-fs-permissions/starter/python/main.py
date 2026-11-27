import sys

import commands
import shell


def main():
    sh = shell.Shell()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in commands.run(sh, words):
            print(out)


main()
