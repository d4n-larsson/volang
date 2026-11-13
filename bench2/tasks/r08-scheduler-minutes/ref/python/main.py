import sys

from clock import is_digits, parse_time
from schedule import add_event, remove_event
from report import agenda_lines, who_line, free_lines, summary_lines


def bad_time(words):
    """The first word that is not a valid time, or None."""
    for w in words:
        if parse_time(w) < 0:
            return w
    return None


def run(words, events):
    cmd = words[0]
    args = words[1:]
    if cmd == "add":
        if len(args) != 4:
            return ["error: bad command"]
        bad = bad_time(args[2:4])
        if bad is not None:
            return ["error: bad time " + bad]
        return [add_event(events, args[0], args[1], parse_time(args[2]), parse_time(args[3]))]
    if cmd == "remove":
        if len(args) != 1:
            return ["error: bad command"]
        return [remove_event(events, args[0])]
    if cmd == "agenda":
        if len(args) != 1:
            return ["error: bad command"]
        return agenda_lines(events, args[0])
    if cmd == "who":
        if len(args) != 2:
            return ["error: bad command"]
        t = parse_time(args[1])
        if t < 0:
            return ["error: bad time " + args[1]]
        return [who_line(events, args[0], t)]
    if cmd == "free":
        if len(args) != 4 or not is_digits(args[3]) or int(args[3]) == 0:
            return ["error: bad command"]
        bad = bad_time(args[1:3])
        if bad is not None:
            return ["error: bad time " + bad]
        lo = parse_time(args[1])
        hi = parse_time(args[2])
        if lo >= hi:
            return ["error: empty interval"]
        return free_lines(events, args[0], lo, hi, int(args[3]))
    if cmd == "summary":
        if len(args) != 0:
            return ["error: bad command"]
        return summary_lines(events)
    return ["error: unknown command " + cmd]


def main():
    events = {}
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, events):
            print(out)


main()
