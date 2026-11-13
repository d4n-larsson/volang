import sys

from ledger import Ledger
from parsing import parse
from report import report


def run(text):
    led = Ledger()
    out = []
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    for n, line in enumerate(lines, 1):
        cmd = parse(line)
        if cmd is None:
            out.append(f"line {n}: invalid")
        elif cmd[0] == "REPORT":
            out.extend(report(led))
        elif cmd[0] == "RECEIVE":
            led.receive(cmd[1], cmd[2], cmd[3])
        elif cmd[0] in ("SHIP", "SCRAP"):
            have = led.stock(cmd[1])
            if have < cmd[2]:
                out.append(f"line {n}: cannot take {cmd[2]} of {cmd[1]}, only {have} in stock")
            elif cmd[0] == "SHIP":
                led.ship(cmd[1], cmd[2], cmd[3])
            else:
                led.scrap(cmd[1], cmd[2])
    out.extend(report(led))
    return out


if __name__ == "__main__":
    sys.stdout.write("".join(l + "\n" for l in run(sys.stdin.read())))
