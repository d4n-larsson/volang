import sys

from orders import Desk
from parsing import parse
from reports import backorder_report, stock_report, value_report


def main():
    desk, out = Desk(), []
    for n, line in enumerate(sys.stdin.read().split("\n"), 1):
        p = parse(line)
        if p == "blank":
            continue
        if p is None:
            out.append(f"line {n}: invalid")
            continue
        cmd, args = p[0], p[1:]
        if cmd == "stock":
            stock_report(desk.stock, out)
        elif cmd == "value":
            value_report(desk.stock, out)
        elif cmd == "backorders":
            backorder_report(desk, out)
        else:
            err = getattr(desk, "do_" + cmd)(*args, out=out)
            if err:
                out.append("error: " + err)
    stock_report(desk.stock, out)
    value_report(desk.stock, out)
    backorder_report(desk, out)
    sys.stdout.write("".join(l + "\n" for l in out))


main()
