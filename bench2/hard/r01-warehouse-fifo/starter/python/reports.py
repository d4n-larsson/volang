"""Read-only views: stock table, valuation, low stock and the summary report."""

from catalog import sorted_skus, sorted_warehouses, unknown_warehouse
from fmt import money, pad_left, pad_right, plural
import stock
import orders


def scope(state, wh):
    """The warehouses a view covers: all of them (wh is None) or one."""
    if wh is None:
        return sorted_warehouses(state)
    return [wh]


def stock_lines(state, wh):
    if wh is not None and wh not in state.stock:
        return [unknown_warehouse(wh)]
    out = []
    total = 0
    for code in scope(state, wh):
        for sku in sorted_skus(state):
            qty = stock.on_hand(state, code, sku)
            if qty > 0:
                out.append(pad_right(code, 8) + pad_right(sku, 10) + pad_left(str(qty), 6))
                total += qty
    if len(out) == 0:
        return ["no stock"]
    out.append("total " + plural(total, "unit"))
    return out


def units_in(state, codes, sku):
    n = 0
    for code in codes:
        n += stock.on_hand(state, code, sku)
    return n


def item_value(state, codes, sku):
    """What the units of sku in the warehouses are worth."""
    return units_in(state, codes, sku) * state.items[sku].cost


def value_lines(state, wh):
    if wh is not None and wh not in state.stock:
        return [unknown_warehouse(wh)]
    codes = scope(state, wh)
    out = []
    total = 0
    for sku in sorted_skus(state):
        qty = units_in(state, codes, sku)
        if qty == 0:
            continue
        worth = item_value(state, codes, sku)
        out.append(sku + ": " + str(qty) + " x " + money(state.items[sku].cost) + " = " + money(worth))
        total += worth
    if len(out) == 0:
        return ["no stock"]
    out.append("total value " + money(total))
    return out


def low_lines(state):
    out = []
    for sku in sorted_skus(state):
        it = state.items[sku]
        have = stock.total_on_hand(state, sku)
        if it.reorder > 0 and have < it.reorder:
            out.append(sku + " " + it.name + ": have " + str(have) + ", reorder at " + str(it.reorder)
                       + ", short " + str(it.reorder - have))
    if len(out) == 0:
        return ["nothing low"]
    return out


def report_lines(state):
    codes = sorted_warehouses(state)
    units = 0
    worth = 0
    for sku in sorted_skus(state):
        units += units_in(state, codes, sku)
        worth += item_value(state, codes, sku)
    first = ("items " + str(len(state.items)) + ", warehouses " + str(len(state.stock)) + ", "
             + plural(units, "unit") + ", value " + money(worth))
    second = ("shipped " + plural(state.shipped_units, "unit") + " in "
              + plural(state.shipments, "shipment"))
    third = ("orders: " + str(orders.count_status(state, "open")) + " open, "
             + str(orders.count_status(state, "fulfilled")) + " fulfilled, "
             + str(orders.count_status(state, "cancelled")) + " cancelled")
    return [first, second, third]
