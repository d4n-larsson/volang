"""Stock on hand per warehouse and item, as FIFO lots."""

from model import Lot


def lots(state, wh, sku):
    shelf = state.stock[wh]
    if sku in shelf:
        return shelf[sku]
    return []


def on_hand(state, wh, sku):
    n = 0
    for lot in lots(state, wh, sku):
        n += lot.qty
    return n


def place(state, wh, sku, receipt, qty, cost):
    """Adds units of a receipt, keeping the lots in receipt order."""
    shelf = state.stock[wh]
    if sku not in shelf:
        shelf[sku] = []
    row = shelf[sku]
    for i in range(len(row)):
        if row[i].receipt == receipt:
            row[i].qty += qty
            return
        if row[i].receipt > receipt:
            row.insert(i, Lot(receipt, qty, cost))
            return
    row.append(Lot(receipt, qty, cost))


def new_lot(state, wh, sku, qty, cost):
    """Puts qty units in as a new lot; returns its receipt number."""
    receipt = state.next_receipt
    state.next_receipt += 1
    place(state, wh, sku, receipt, qty, cost)
    return receipt


def take(state, wh, sku, qty):
    """Removes qty units, oldest receipt first; the caller has checked that they are there.
    Returns the pieces taken as [Lot]."""
    row = state.stock[wh][sku]
    pieces = []
    while qty > 0:
        lot = row[0]
        n = min(qty, lot.qty)
        pieces.append(Lot(lot.receipt, n, lot.cost))
        lot.qty -= n
        qty -= n
        if lot.qty == 0:
            row.pop(0)
    if len(row) == 0:
        del state.stock[wh][sku]
    return pieces


def cost_of(pieces):
    total = 0
    for p in pieces:
        total += p.qty * p.cost
    return total


def total_on_hand(state, sku):
    n = 0
    for wh in state.stock:
        n += on_hand(state, wh, sku)
    return n


def has_stock(state, wh):
    for sku in state.stock[wh]:
        if on_hand(state, wh, sku) > 0:
            return True
    return False


def short_message(state, wh, sku):
    return "error: only " + str(on_hand(state, wh, sku)) + " " + sku + " in " + wh
