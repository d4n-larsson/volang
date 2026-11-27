"""Items and warehouses."""

from model import Item
from fmt import money


def add_warehouse(state, code):
    if code in state.stock:
        return "error: warehouse " + code + " exists"
    state.stock[code] = {}
    return "warehouse " + code + " added"


def add_item(state, sku, name, cost):
    if sku in state.items:
        return "error: item " + sku + " exists"
    state.items[sku] = Item(sku, name, cost)
    return "item " + sku + " added"


def set_cost(state, sku, cost):
    if sku not in state.items:
        return unknown_item(sku)
    state.items[sku].cost = cost
    return "item " + sku + " cost " + money(cost)


def set_reorder(state, sku, point):
    if sku not in state.items:
        return unknown_item(sku)
    state.items[sku].reorder = point
    return "item " + sku + " reorder point " + str(point)


def unknown_item(sku):
    return "error: unknown item " + sku


def unknown_warehouse(code):
    return "error: unknown warehouse " + code


def sorted_skus(state):
    return sorted(state.items.keys())


def sorted_warehouses(state):
    return sorted(state.stock.keys())


def item_lines(state):
    if len(state.items) == 0:
        return ["no items"]
    out = []
    for sku in sorted_skus(state):
        it = state.items[sku]
        out.append(sku + " " + it.name + " cost " + money(it.cost) + " reorder " + str(it.reorder))
    return out
