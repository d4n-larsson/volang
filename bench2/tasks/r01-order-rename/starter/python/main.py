import sys

from catalog import add_item, restock_item, set_item_price
from orders import place_order, cancel_order
from report import money, placed_line, show_order, full_report


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def run(words, catalog, orders):
    cmd = words[0]
    args = words[1:]
    if cmd == "add-item":
        if len(args) != 4 or not is_number(args[2]) or not is_number(args[3]):
            return ["error: bad command"]
        return [add_item(catalog, args[0], args[1], int(args[2]), int(args[3]))]
    if cmd == "restock":
        if len(args) != 2 or not is_number(args[1]):
            return ["error: bad command"]
        return [restock_item(catalog, args[0], int(args[1]))]
    if cmd == "price":
        if len(args) != 2 or not is_number(args[1]):
            return ["error: bad command"]
        return [set_item_price(catalog, args[0], int(args[1]), money)]
    if cmd == "order":
        if len(args) < 3 or len(args) % 2 != 1:
            return ["error: bad command"]
        pairs = []
        for i in range(1, len(args), 2):
            if not is_number(args[i + 1]) or int(args[i + 1]) == 0:
                return ["error: bad command"]
            pairs.append((args[i], int(args[i + 1])))
        order, error = place_order(catalog, orders, args[0], pairs)
        if order is None:
            return [error]
        return [placed_line(order)]
    if cmd == "cancel":
        if len(args) != 1:
            return ["error: bad command"]
        return [cancel_order(catalog, orders, args[0])]
    if cmd == "show":
        if len(args) != 1:
            return ["error: bad command"]
        if args[0] not in orders:
            return ["error: unknown order " + args[0]]
        return show_order(orders[args[0]])
    if cmd == "report":
        if len(args) != 0:
            return ["error: bad command"]
        return full_report(catalog, orders)
    return ["error: unknown command " + cmd]


def main():
    catalog = {}
    orders = {}
    for line in sys.stdin.read().split("\n"):
        words = line.split(" ")
        words = [w for w in words if w != ""]
        if not words:
            continue
        for out in run(words, catalog, orders):
            print(out)


main()
