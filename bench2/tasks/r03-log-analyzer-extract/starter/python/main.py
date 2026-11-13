import sys

from counts import stats, levels, errors
from timeline import busiest, in_range, span
from search import grep


def run(words, logs):
    cmd = words[0]
    args = words[1:]
    if cmd == "stats" and len(args) == 0:
        return stats(logs)
    if cmd == "levels" and len(args) == 0:
        return levels(logs)
    if cmd == "errors" and len(args) == 0:
        return errors(logs)
    if cmd == "busiest" and len(args) == 0:
        return busiest(logs)
    if cmd == "range" and len(args) == 4:
        return in_range(logs, args)
    if cmd == "span" and len(args) == 0:
        return span(logs)
    if cmd == "grep" and len(args) == 1:
        return grep(logs, args[0])
    if cmd in ("stats", "levels", "errors", "busiest", "range", "span", "grep"):
        return ["error: bad command"]
    return ["error: unknown command " + cmd]


def main():
    logs = []
    commands = []
    in_commands = False
    for line in sys.stdin.read().split("\n"):
        if line == "---" and not in_commands:
            in_commands = True
        elif line.strip(" ") == "":
            continue
        elif in_commands:
            commands.append(line)
        else:
            logs.append(line)
    for line in commands:
        words = [w for w in line.split(" ") if w != ""]
        for out in run(words, logs):
            print(out)


main()
