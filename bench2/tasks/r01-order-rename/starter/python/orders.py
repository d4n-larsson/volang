from catalog import find_item

DISCOUNT_FROM = 10000
DISCOUNT_PERCENT = 5


class Line:
    def __init__(self, sku, qty, price):
        self.sku = sku
        self.qty = qty
        self.price = price


class Order:
    def __init__(self, order_id, lines, total, discount):
        self.order_id = order_id
        self.lines = lines
        self.total = total
        self.discount = discount
        self.cancelled = False


def requested_by_sku(pairs):
    wanted = {}
    for sku, qty in pairs:
        wanted[sku] = wanted.get(sku, 0) + qty
    return wanted


def place_order(catalog, orders, order_id, pairs):
    """Returns (order or None, error message)."""
    if order_id in orders:
        return None, "error: order " + order_id + " exists"
    wanted = requested_by_sku(pairs)
    for sku in wanted:
        item = find_item(catalog, sku)
        if item is None:
            return None, "error: unknown item " + sku
        if wanted[sku] > item.stock:
            return None, "error: not enough stock for item " + sku
    lines = []
    subtotal = 0
    for sku, qty in pairs:
        item = find_item(catalog, sku)
        item.stock -= qty
        item.sold += qty
        lines.append(Line(sku, qty, item.price))
        subtotal += qty * item.price
    discount = 0
    if subtotal >= DISCOUNT_FROM:
        discount = subtotal * DISCOUNT_PERCENT // 100
    order = Order(order_id, lines, subtotal - discount, discount)
    orders[order_id] = order
    return order, ""


def cancel_order(catalog, orders, order_id):
    order = orders.get(order_id)
    if order is None:
        return "error: unknown order " + order_id
    if order.cancelled:
        return "error: order " + order_id + " already cancelled"
    for line in order.lines:
        item = find_item(catalog, line.sku)
        item.stock += line.qty
        item.sold -= line.qty
    order.cancelled = True
    return "order " + order_id + " cancelled"


def units(order):
    return sum(line.qty for line in order.lines)
