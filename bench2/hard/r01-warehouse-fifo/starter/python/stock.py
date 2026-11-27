"""Quantities on hand per warehouse and item."""


def on_hand(state, wh, sku):
    shelf = state.stock[wh]
    if sku in shelf:
        return shelf[sku]
    return 0


def put(state, wh, sku, qty):
    shelf = state.stock[wh]
    shelf[sku] = on_hand(state, wh, sku) + qty


def take(state, wh, sku, qty):
    """Removes qty units; the caller has checked that they are there."""
    shelf = state.stock[wh]
    left = on_hand(state, wh, sku) - qty
    if left == 0:
        del shelf[sku]
    else:
        shelf[sku] = left


def total_on_hand(state, sku):
    n = 0
    for wh in state.stock:
        n += on_hand(state, wh, sku)
    return n


def has_stock(state, wh):
    for sku in state.stock[wh]:
        if state.stock[wh][sku] > 0:
            return True
    return False


def short_message(state, wh, sku):
    return "error: only " + str(on_hand(state, wh, sku)) + " " + sku + " in " + wh
