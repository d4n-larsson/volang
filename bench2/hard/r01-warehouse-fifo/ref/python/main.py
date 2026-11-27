import sys

from model import State
from parse import is_number, is_positive, is_delta, delta_value
from catalog import add_warehouse, add_item, set_cost, set_reorder, item_lines, unknown_warehouse
import moves
import orders
import reports
import stock

BAD = ["error: bad command"]


def drop_warehouse(state, code):
    if code not in state.stock:
        return unknown_warehouse(code)
    if stock.has_stock(state, code):
        return "error: warehouse " + code + " has stock"
    if orders.has_open_orders(state, code):
        return "error: warehouse " + code + " has open orders"
    del state.stock[code]
    return "warehouse " + code + " dropped"


def catalog_command(state, cmd, args):
    if cmd == "warehouse":
        if len(args) != 1:
            return BAD
        return [add_warehouse(state, args[0])]
    if cmd == "drop":
        if len(args) != 1:
            return BAD
        return [drop_warehouse(state, args[0])]
    if cmd == "item":
        if len(args) != 3 or not is_number(args[2]):
            return BAD
        return [add_item(state, args[0], args[1], int(args[2]))]
    if cmd == "cost":
        if len(args) != 2 or not is_number(args[1]):
            return BAD
        return [set_cost(state, args[0], int(args[1]))]
    if cmd == "reorder":
        if len(args) != 2 or not is_number(args[1]):
            return BAD
        return [set_reorder(state, args[0], int(args[1]))]
    if len(args) != 0:
        return BAD
    return item_lines(state)


def move_command(state, cmd, args):
    if cmd == "receive":
        if len(args) < 3 or len(args) > 4 or not is_positive(args[2]):
            return BAD
        if len(args) == 4 and not is_number(args[3]):
            return BAD
        cost = int(args[3]) if len(args) == 4 else None
        return [moves.receive(state, args[0], args[1], int(args[2]), cost)]
    if cmd == "ship":
        if len(args) != 3 or not is_positive(args[2]):
            return BAD
        return [moves.ship(state, args[0], args[1], int(args[2]))]
    if cmd == "transfer":
        if len(args) != 4 or not is_positive(args[3]):
            return BAD
        return [moves.transfer(state, args[0], args[1], args[2], int(args[3]))]
    if cmd == "adjust":
        if len(args) != 4 or not is_delta(args[2]):
            return BAD
        return [moves.adjust(state, args[0], args[1], delta_value(args[2]), args[3])]
    if len(args) != 3 or not is_number(args[2]):
        return BAD
    return [moves.count(state, args[0], args[1], int(args[2]))]


def order_command(state, cmd, args):
    if cmd == "order":
        if len(args) != 2:
            return BAD
        return [orders.open_order(state, args[0], args[1])]
    if cmd == "line":
        if len(args) != 3 or not is_positive(args[2]):
            return BAD
        return [orders.add_line(state, args[0], args[1], int(args[2]))]
    if cmd == "orders":
        if len(args) != 0:
            return BAD
        return orders.order_lines(state)
    if len(args) != 1:
        return BAD
    if cmd == "fulfil":
        return [orders.fulfil(state, args[0])]
    return [orders.cancel(state, args[0])]


def report_command(state, cmd, args):
    if cmd == "stock" or cmd == "value":
        if len(args) > 1:
            return BAD
        wh = args[0] if len(args) == 1 else None
        if cmd == "stock":
            return reports.stock_lines(state, wh)
        return reports.value_lines(state, wh)
    if cmd == "history":
        if len(args) != 1:
            return BAD
        return moves.history(state, args[0])
    if cmd == "lots":
        if len(args) != 2:
            return BAD
        return moves.lot_lines(state, args[0], args[1])
    if len(args) != 0:
        return BAD
    if cmd == "low":
        return reports.low_lines(state)
    return reports.report_lines(state)


GROUPS = [
    (["warehouse", "drop", "item", "cost", "reorder", "items"], catalog_command),
    (["receive", "ship", "transfer", "adjust", "count"], move_command),
    (["order", "line", "orders", "fulfil", "cancel"], order_command),
    (["stock", "value", "history", "lots", "low", "report"], report_command),
]


def run(state, words):
    cmd = words[0]
    for names, handler in GROUPS:
        if cmd in names:
            return handler(state, cmd, words[1:])
    return ["error: unknown command " + cmd]


def main():
    state = State()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if len(words) == 0:
            continue
        for out in run(state, words):
            print(out)


main()
