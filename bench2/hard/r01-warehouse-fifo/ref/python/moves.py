"""Stock movements: receipts, shipments, transfers and adjustments, and their history."""

from model import Move
from catalog import unknown_item, unknown_warehouse
from fmt import signed, money, plural
from parse import is_reason
import stock


def record(state, kind, wh, sku, delta, note, lot, amount):
    state.moves.append(Move(state.next_seq, kind, wh, sku, delta, note, lot, amount))
    state.next_seq += 1


def lot_text(lot, cost):
    return "lot " + str(lot) + " at " + money(cost)


def cost_text(cost):
    return "cost " + money(cost)


def check_place(state, wh, sku):
    """The error for an unknown warehouse or item, or None."""
    if wh not in state.stock:
        return unknown_warehouse(wh)
    if sku not in state.items:
        return unknown_item(sku)
    return None


def receive(state, wh, sku, qty, cost):
    """cost None: the item's standard cost."""
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    if cost is None:
        cost = state.items[sku].cost
    lot = stock.new_lot(state, wh, sku, qty, cost)
    record(state, "receive", wh, sku, qty, "", lot, cost)
    return ("received " + str(qty) + " " + sku + " into " + wh + ", now " + str(stock.on_hand(state, wh, sku))
            + ", " + lot_text(lot, cost))


def ship(state, wh, sku, qty):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    if stock.on_hand(state, wh, sku) < qty:
        return stock.short_message(state, wh, sku)
    cost = stock.cost_of(stock.take(state, wh, sku, qty))
    record(state, "ship", wh, sku, -qty, "", 0, cost)
    state.shipments += 1
    state.shipped_units += qty
    state.shipped_cost += cost
    return ("shipped " + str(qty) + " " + sku + " from " + wh + ", now " + str(stock.on_hand(state, wh, sku))
            + ", " + cost_text(cost))


def transfer(state, src, dst, sku, qty):
    if src not in state.stock:
        return unknown_warehouse(src)
    if dst not in state.stock:
        return unknown_warehouse(dst)
    if src == dst:
        return "error: same warehouse " + src
    if sku not in state.items:
        return unknown_item(sku)
    if stock.on_hand(state, src, sku) < qty:
        return stock.short_message(state, src, sku)
    pieces = stock.take(state, src, sku, qty)
    for p in pieces:
        stock.place(state, dst, sku, p.receipt, p.qty, p.cost)
    cost = stock.cost_of(pieces)
    record(state, "transfer-out", src, sku, -qty, "to " + dst, 0, cost)
    record(state, "transfer-in", dst, sku, qty, "from " + src, 0, cost)
    return "transferred " + str(qty) + " " + sku + " from " + src + " to " + dst + ", " + cost_text(cost)


def change(state, wh, sku, delta, reason):
    """Applies a checked adjustment of delta units (not 0); returns what it adds to the message."""
    if delta > 0:
        cost = state.items[sku].cost
        lot = stock.new_lot(state, wh, sku, delta, cost)
        record(state, "adjust", wh, sku, delta, reason, lot, cost)
        return lot_text(lot, cost)
    cost = stock.cost_of(stock.take(state, wh, sku, -delta))
    record(state, "adjust", wh, sku, delta, reason, 0, cost)
    return cost_text(cost)


def adjust(state, wh, sku, delta, reason):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    if not is_reason(reason):
        return "error: unknown reason " + reason
    if delta < 0 and stock.on_hand(state, wh, sku) < -delta:
        return stock.short_message(state, wh, sku)
    extra = change(state, wh, sku, delta, reason)
    return ("adjusted " + sku + " in " + wh + " by " + signed(delta) + " (" + reason + "), now "
            + str(stock.on_hand(state, wh, sku)) + ", " + extra)


def count(state, wh, sku, qty):
    """A stock-take: the shelf holds qty units, adjusted with reason `count`."""
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    was = stock.on_hand(state, wh, sku)
    if was == qty:
        return "counted " + sku + " in " + wh + ": no change"
    extra = change(state, wh, sku, qty - was, "count")
    return "counted " + sku + " in " + wh + ": was " + str(was) + ", now " + str(qty) + ", " + extra


def history_line(m):
    line = "#" + str(m.seq) + " " + m.kind + " " + m.wh + " " + signed(m.delta)
    if m.note != "":
        line += " (" + m.note + ")"
    if m.lot > 0:
        return line + " " + lot_text(m.lot, m.amount)
    return line + " " + cost_text(m.amount)


def history(state, sku):
    if sku not in state.items:
        return [unknown_item(sku)]
    out = [history_line(m) for m in state.moves if m.sku == sku]
    if len(out) == 0:
        return ["no movements for " + sku]
    return out


def lot_lines(state, wh, sku):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return [problem]
    row = stock.lots(state, wh, sku)
    if len(row) == 0:
        return ["no lots of " + sku + " in " + wh]
    out = []
    for lot in row:
        out.append("lot " + str(lot.receipt) + ": " + str(lot.qty) + " x " + money(lot.cost) + " = "
                   + money(lot.qty * lot.cost))
    out.append("total " + plural(stock.on_hand(state, wh, sku), "unit") + " = " + money(stock.cost_of(row)))
    return out
