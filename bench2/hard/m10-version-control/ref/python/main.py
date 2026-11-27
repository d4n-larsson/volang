import sys

from repo import Repo, RepoError
from commands import COMMANDS


def run_line(repo, fields):
    word, args = fields[0], fields[1:]
    if word not in COMMANDS or not COMMANDS[word][1](len(args)):
        return ["error: bad command"]
    try:
        return COMMANDS[word][0](repo, args)
    except RepoError as e:
        return [f"error: {e}"]


def main():
    repo = Repo()
    out = []
    for line in sys.stdin.read().split("\n"):
        fields = line.split()
        if fields:
            out += run_line(repo, fields)
    sys.stdout.write("".join(s + "\n" for s in out))


main()
