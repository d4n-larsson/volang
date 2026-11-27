"""Sales orders: opened at one warehouse, filled with lines, fulfilled all at once."""

from model import Order, OrderLine
from catalog import unknown_item, unknown_warehouse
from fmt import plural, money
import stock
import moves


def unknown_order(order_id):
    return "error: unknown order " + order_id


def not_open(order):
    return "error: order " + order.order_id + " is " + order.status


def open_order(state, order_id, wh):
    if order_id in state.orders:
        return "error: order " + order_id + " exists"
    if wh not in state.stock:
        return unknown_warehouse(wh)
    state.orders[order_id] = Order(order_id, wh)
    return "order " + order_id + " opened at " + wh


def add_line(state, order_id, sku, qty):
    if order_id not in state.orders:
        return unknown_order(order_id)
    order = state.orders[order_id]
    if order.status != "open":
        return not_open(order)
    if sku not in state.items:
        return unknown_item(sku)
    order.lines.append(OrderLine(sku, qty))
    return "order " + order_id + " line " + str(len(order.lines)) + ": " + str(qty) + " " + sku


def units(order):
    n = 0
    for line in order.lines:
        n += line.qty
    return n


def demand(order):
    """Units wanted per SKU, in the order the SKUs first appear in the lines."""
    want = {}
    for line in order.lines:
        if line.sku in want:
            want[line.sku] += line.qty
        else:
            want[line.sku] = line.qty
    return want


def shortage(state, order):
    """The error for the first SKU the warehouse cannot cover, or None."""
    want = demand(order)
    for sku in want:
        have = stock.on_hand(state, order.wh, sku)
        if have < want[sku]:
            return ("error: order " + order.order_id + " short of " + sku + ": need "
                    + str(want[sku]) + ", have " + str(have))
    return None


def fulfil(state, order_id):
    if order_id not in state.orders:
        return unknown_order(order_id)
    order = state.orders[order_id]
    if order.status != "open":
        return not_open(order)
    if len(order.lines) == 0:
        return "error: order " + order_id + " has no lines"
    problem = shortage(state, order)
    if problem is not None:
        return problem
    total = 0
    for line in order.lines:
        cost = stock.cost_of(stock.take(state, order.wh, line.sku, line.qty))
        moves.record(state, "ship", order.wh, line.sku, -line.qty, "order " + order_id, 0, cost)
        total += cost
    order.status = "fulfilled"
    state.shipments += 1
    state.shipped_units += units(order)
    state.shipped_cost += total
    return ("order " + order_id + " fulfilled: " + plural(len(order.lines), "line") + ", "
            + plural(units(order), "unit") + ", cost " + money(total))


def cancel(state, order_id):
    if order_id not in state.orders:
        return unknown_order(order_id)
    order = state.orders[order_id]
    if order.status != "open":
        return not_open(order)
    order.status = "cancelled"
    return "order " + order_id + " cancelled"


def order_lines(state):
    if len(state.orders) == 0:
        return ["no orders"]
    out = []
    for order_id in state.orders:
        order = state.orders[order_id]
        out.append(order_id + " " + order.wh + " " + order.status + ": " + plural(len(order.lines), "line")
                   + ", " + plural(units(order), "unit"))
    return out


def has_open_orders(state, wh):
    for order_id in state.orders:
        order = state.orders[order_id]
        if order.wh == wh and order.status == "open":
            return True
    return False


def count_status(state, status):
    n = 0
    for order_id in state.orders:
        if state.orders[order_id].status == status:
            n += 1
    return n
