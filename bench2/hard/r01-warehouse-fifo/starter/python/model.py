"""The records shared by every module."""


class Item:
    def __init__(self, sku, name, cost):
        self.sku = sku
        self.name = name
        self.cost = cost          # standard unit cost in cents
        self.reorder = 0          # reorder point; 0 means none


class Order:
    def __init__(self, order_id, wh):
        self.order_id = order_id
        self.wh = wh
        self.lines = []           # [OrderLine]
        self.status = "open"      # open, fulfilled or cancelled


class OrderLine:
    def __init__(self, sku, qty):
        self.sku = sku
        self.qty = qty


class Move:
    """One stock movement: delta is positive for stock coming in."""

    def __init__(self, seq, kind, wh, sku, delta, note):
        self.seq = seq
        self.kind = kind
        self.wh = wh
        self.sku = sku
        self.delta = delta
        self.note = note


class State:
    def __init__(self):
        self.items = {}           # sku -> Item
        self.stock = {}           # warehouse code -> {sku -> quantity}
        self.orders = {}          # order id -> Order, in creation order
        self.moves = []           # [Move], in sequence order
        self.next_seq = 1
        self.shipments = 0
        self.shipped_units = 0
