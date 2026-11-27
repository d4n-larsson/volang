import sys

from fstree import Tree, FsError
from commands import COMMANDS


def run_line(t, fields):
    word, args = fields[0], fields[1:]
    if word not in COMMANDS:
        return [f"{word}: unknown command"]
    flag_text, f, ok = COMMANDS[word]
    flag = False
    if flag_text is not None and args and args[0] == flag_text:
        flag, args = True, args[1:]
    if not ok(args):
        return [f"{word}: usage"]
    try:
        return f(t, flag, args)
    except FsError as e:
        return [f"{word}: {args[0]}: {e}"]


def main():
    t = Tree()
    out = []
    for line in sys.stdin.read().split("\n"):
        fields = line.split()
        if fields:
            for s in run_line(t, fields):
                out.append(s if s.endswith("\n") else s + "\n")
    sys.stdout.write("".join(out))


main()
