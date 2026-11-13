from shop import Bulk, unknown_product
from money import percent_of


class Priced:
    """One cart line with its discounts worked out."""

    def __init__(self, sku, qty, price):
        self.sku = sku
        self.qty = qty
        self.price = price
        self.gross = qty * price
        self.free_qty = 0
        self.free_amount = 0
        self.bulk_pct = 0
        self.bulk_amount = 0
        self.net = self.gross


def set_promo(shop, sku, buy):
    if sku not in shop.catalog:
        return unknown_product(sku)
    shop.promos[sku] = buy
    return "promo " + sku + ": buy " + str(buy) + " get 1 free"


def set_bulk(shop, sku, min_qty, pct):
    if sku not in shop.catalog:
        return unknown_product(sku)
    shop.bulks[sku] = Bulk(min_qty, pct)
    return "bulk " + sku + ": " + str(min_qty) + " or more " + str(pct) + "% off"


def define_coupon(shop, code, pct):
    shop.coupons[code] = pct
    return "coupon " + code + ": " + str(pct) + "% off"


def use_coupon(shop, code):
    if code not in shop.coupons:
        return "error: unknown coupon " + code
    shop.active = code
    return "coupon " + code + " active"


def price_line(shop, line):
    p = Priced(line.sku, line.qty, shop.catalog[line.sku].price)
    if line.sku in shop.promos:
        p.free_qty = line.qty // (shop.promos[line.sku] + 1)
        p.free_amount = p.free_qty * p.price
    base = p.gross - p.free_amount
    if line.sku in shop.bulks and line.qty >= shop.bulks[line.sku].min_qty:
        p.bulk_pct = shop.bulks[line.sku].pct
        p.bulk_amount = percent_of(base, p.bulk_pct)
    p.net = base - p.bulk_amount
    return p


def coupon_discount(shop, subtotal):
    """The active coupon's discount on subtotal (0 without a coupon)."""
    if shop.active == "":
        return 0
    return percent_of(subtotal, shop.coupons[shop.active])
