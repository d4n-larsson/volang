import sys

from garage import Garage, open_level, park, occupancy, parked
from fees import rates, VEHICLE_TYPES
from billing import exit_vehicle, quote, report


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def run(words, garage):
    cmd = words[0]
    args = words[1:]
    bad = ["error: bad command"]
    if cmd == "level":
        if len(args) != 2 or not is_number(args[0]) or not is_number(args[1]):
            return bad
        number = int(args[0])
        spots = int(args[1])
        if number < 1 or spots < 1 or spots > 99:
            return bad
        return [open_level(garage, number, spots)]
    if cmd == "enter":
        if (len(args) != 2 and len(args) != 3) or not is_number(args[1]):
            return bad
        kind = args[2] if len(args) == 3 else "car"
        if kind not in VEHICLE_TYPES:
            return ["error: unknown vehicle type " + kind]
        return [park(garage, args[0], int(args[1]), kind)]
    if cmd == "exit":
        if len(args) != 2 or not is_number(args[1]):
            return bad
        return [exit_vehicle(garage, args[0], int(args[1]))]
    if cmd == "quote":
        if len(args) != 2 or not is_number(args[1]):
            return bad
        return [quote(garage, args[0], int(args[1]))]
    if cmd == "occupancy":
        if len(args) != 0:
            return bad
        return occupancy(garage)
    if cmd == "parked":
        if len(args) != 0:
            return bad
        return parked(garage)
    if cmd == "report":
        if len(args) != 1 or not is_number(args[0]):
            return bad
        return report(garage, int(args[0]))
    if cmd == "rates":
        if len(args) != 0:
            return bad
        return rates()
    return ["error: unknown command " + cmd]


def main():
    garage = Garage()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, garage):
            print(out)


main()
