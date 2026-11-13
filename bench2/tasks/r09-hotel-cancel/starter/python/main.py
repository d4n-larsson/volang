import sys

from rooms import add_type, set_rate
from bookings import book
from billing import money, invoice_lines
from report import avail_line, occupancy_lines, report_lines


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def run(words, types, bookings):
    cmd = words[0]
    args = words[1:]
    if cmd == "type":
        if len(args) != 3 or not is_number(args[1]) or not is_number(args[2]):
            return ["error: bad command"]
        return [add_type(types, args[0], int(args[1]), int(args[2]))]
    if cmd == "rate":
        if len(args) != 2 or not is_number(args[1]):
            return ["error: bad command"]
        return [set_rate(types, args[0], int(args[1]), money)]
    if cmd == "book":
        if len(args) != 5 or not is_number(args[3]) or not is_number(args[4]):
            return ["error: bad command"]
        return [book(types, bookings, args[0], args[1], args[2], int(args[3]), int(args[4]))]
    if cmd == "avail":
        if len(args) != 3 or not is_number(args[1]) or not is_number(args[2]):
            return ["error: bad command"]
        if args[0] not in types:
            return ["error: unknown type " + args[0]]
        if int(args[1]) >= int(args[2]):
            return ["error: bad dates"]
        return [avail_line(types, bookings, args[0], int(args[1]), int(args[2]))]
    if cmd == "occupancy":
        if len(args) != 1 or not is_number(args[0]):
            return ["error: bad command"]
        return occupancy_lines(types, bookings, int(args[0]))
    if cmd == "invoice":
        if len(args) != 1:
            return ["error: bad command"]
        if args[0] not in bookings:
            return ["error: unknown booking " + args[0]]
        return invoice_lines(bookings[args[0]])
    if cmd == "report":
        if len(args) != 0:
            return ["error: bad command"]
        return report_lines(types, bookings)
    return ["error: unknown command " + cmd]


def main():
    types = {}
    bookings = {}
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, types, bookings):
            print(out)


main()
