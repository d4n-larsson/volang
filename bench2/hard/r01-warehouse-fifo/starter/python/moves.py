"""Stock movements: receipts, shipments, transfers and adjustments, and their history."""

from model import Move
from catalog import unknown_item, unknown_warehouse
from fmt import signed
from parse import is_reason
import stock


def record(state, kind, wh, sku, delta, note):
    state.moves.append(Move(state.next_seq, kind, wh, sku, delta, note))
    state.next_seq += 1


def check_place(state, wh, sku):
    """The error for an unknown warehouse or item, or None."""
    if wh not in state.stock:
        return unknown_warehouse(wh)
    if sku not in state.items:
        return unknown_item(sku)
    return None


def receive(state, wh, sku, qty):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    stock.put(state, wh, sku, qty)
    record(state, "receive", wh, sku, qty, "")
    return "received " + str(qty) + " " + sku + " into " + wh + ", now " + str(stock.on_hand(state, wh, sku))


def ship(state, wh, sku, qty):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    if stock.on_hand(state, wh, sku) < qty:
        return stock.short_message(state, wh, sku)
    stock.take(state, wh, sku, qty)
    record(state, "ship", wh, sku, -qty, "")
    state.shipments += 1
    state.shipped_units += qty
    return "shipped " + str(qty) + " " + sku + " from " + wh + ", now " + str(stock.on_hand(state, wh, sku))


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
    stock.take(state, src, sku, qty)
    stock.put(state, dst, sku, qty)
    record(state, "transfer-out", src, sku, -qty, "to " + dst)
    record(state, "transfer-in", dst, sku, qty, "from " + src)
    return "transferred " + str(qty) + " " + sku + " from " + src + " to " + dst


def change(state, wh, sku, delta, reason):
    """Applies a checked adjustment of delta units (not 0)."""
    if delta > 0:
        stock.put(state, wh, sku, delta)
    else:
        stock.take(state, wh, sku, -delta)
    record(state, "adjust", wh, sku, delta, reason)


def adjust(state, wh, sku, delta, reason):
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    if not is_reason(reason):
        return "error: unknown reason " + reason
    if delta < 0 and stock.on_hand(state, wh, sku) < -delta:
        return stock.short_message(state, wh, sku)
    change(state, wh, sku, delta, reason)
    return ("adjusted " + sku + " in " + wh + " by " + signed(delta) + " (" + reason + "), now "
            + str(stock.on_hand(state, wh, sku)))


def count(state, wh, sku, qty):
    """A stock-take: the shelf holds qty units, adjusted with reason `count`."""
    problem = check_place(state, wh, sku)
    if problem is not None:
        return problem
    was = stock.on_hand(state, wh, sku)
    if was == qty:
        return "counted " + sku + " in " + wh + ": no change"
    change(state, wh, sku, qty - was, "count")
    return "counted " + sku + " in " + wh + ": was " + str(was) + ", now " + str(qty)


def history_line(m):
    line = "#" + str(m.seq) + " " + m.kind + " " + m.wh + " " + signed(m.delta)
    if m.note != "":
        line += " (" + m.note + ")"
    return line


def history(state, sku):
    if sku not in state.items:
        return [unknown_item(sku)]
    out = [history_line(m) for m in state.moves if m.sku == sku]
    if len(out) == 0:
        return ["no movements for " + sku]
    return out
