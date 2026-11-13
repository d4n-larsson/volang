class Item:
    def __init__(self, sku, name, price, stock):
        self.sku = sku
        self.name = name
        self.price = price
        self.stock = stock
        self.sold = 0


def find_item(catalog, sku):
    return catalog.get(sku)


def add_item(catalog, sku, name, price, stock):
    if sku in catalog:
        return "error: item " + sku + " exists"
    catalog[sku] = Item(sku, name, price, stock)
    return "item " + sku + " added"


def restock_item(catalog, sku, n):
    item = find_item(catalog, sku)
    if item is None:
        return "error: unknown item " + sku
    item.stock += n
    return "item " + sku + " stock " + str(item.stock)


def set_item_price(catalog, sku, price, money):
    item = find_item(catalog, sku)
    if item is None:
        return "error: unknown item " + sku
    item.price = price
    return "item " + sku + " price " + money(price)


def sorted_items(catalog):
    return [catalog[k] for k in sorted(catalog)]
