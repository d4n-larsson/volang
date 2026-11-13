class Product:
    def __init__(self, sku, name, price):
        self.sku = sku
        self.name = name
        self.price = price
        self.sold = 0
        self.net = 0


class Line:
    def __init__(self, sku, qty):
        self.sku = sku
        self.qty = qty


class Bulk:
    def __init__(self, min_qty, pct):
        self.min_qty = min_qty
        self.pct = pct


class Shop:
    def __init__(self):
        self.catalog = {}
        self.cart = []
        self.promos = {}
        self.bulks = {}
        self.coupons = {}
        self.active = ""
        self.sales = 0
        self.revenue = 0
        self.discounts = 0


def find_line(shop, sku):
    for line in shop.cart:
        if line.sku == sku:
            return line
    return None


def unknown_product(sku):
    return "error: unknown product " + sku


def sorted_products(shop):
    return [shop.catalog[sku] for sku in sorted(shop.catalog)]
