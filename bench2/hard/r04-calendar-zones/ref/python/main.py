import sys

from commands import run
from model import Calendar


def main():
    cal = Calendar()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(cal, words):
            print(out)


main()
