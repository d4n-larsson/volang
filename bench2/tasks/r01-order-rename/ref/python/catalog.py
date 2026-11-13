class Product:
    def __init__(self, sku, name, price, stock):
        self.sku = sku
        self.name = name
        self.price = price
        self.stock = stock
        self.sold = 0


def find_product(catalog, sku):
    return catalog.get(sku)


def add_product(catalog, sku, name, price, stock):
    if sku in catalog:
        return "error: product " + sku + " exists"
    catalog[sku] = Product(sku, name, price, stock)
    return "product " + sku + " added"


def restock_product(catalog, sku, n):
    product = find_product(catalog, sku)
    if product is None:
        return "error: unknown product " + sku
    product.stock += n
    return "product " + sku + " stock " + str(product.stock)


def set_product_price(catalog, sku, price, money):
    product = find_product(catalog, sku)
    if product is None:
        return "error: unknown product " + sku
    product.price = price
    return "product " + sku + " price " + money(price)


def sorted_products(catalog):
    return [catalog[k] for k in sorted(catalog)]
