import sys

import commands
import tracker


def main():
    t = tracker.Tracker()
    out = []
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        # Every command, even one that fails, takes one tick of the clock.
        t.clock += 1
        out.extend(commands.run(t, words))
    sys.stdout.write("".join(o + "\n" for o in out))


main()
