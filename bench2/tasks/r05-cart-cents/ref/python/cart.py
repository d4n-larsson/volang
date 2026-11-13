from shop import Product, Line, find_line, unknown_product
from money import money


def add_product(shop, sku, name, price):
    if sku in shop.catalog:
        return "error: product " + sku + " exists"
    shop.catalog[sku] = Product(sku, name, price)
    return "product " + sku + " added"


def set_price(shop, sku, price):
    if sku not in shop.catalog:
        return unknown_product(sku)
    shop.catalog[sku].price = price
    return "product " + sku + " price " + money(price)


def add_to_cart(shop, sku, qty):
    if sku not in shop.catalog:
        return unknown_product(sku)
    line = find_line(shop, sku)
    if line is None:
        line = Line(sku, 0)
        shop.cart.append(line)
    line.qty += qty
    return "cart " + sku + " x" + str(line.qty)


def remove_from_cart(shop, sku, qty):
    line = find_line(shop, sku)
    if line is None:
        return "error: " + sku + " not in cart"
    if qty > line.qty:
        return "error: only " + str(line.qty) + " " + sku + " in cart"
    line.qty -= qty
    if line.qty == 0:
        shop.cart = [x for x in shop.cart if x.sku != sku]
        return "cart " + sku + " removed"
    return "cart " + sku + " x" + str(line.qty)


def show_cart(shop):
    if not shop.cart:
        return ["cart empty"]
    out = []
    units = 0
    gross = 0
    for line in shop.cart:
        price = shop.catalog[line.sku].price
        out.append(line.sku + " x" + str(line.qty) + " @ " + money(price) + " = " + money(line.qty * price))
        units += line.qty
        gross += line.qty * price
    out.append("items " + str(units) + ", gross " + money(gross))
    return out
