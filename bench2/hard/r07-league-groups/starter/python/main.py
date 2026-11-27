import sys

from model import League
from commands import run


def main():
    league = League()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(league, words):
            print(out)


main()
