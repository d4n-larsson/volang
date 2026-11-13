from shop import sorted_products
from money import money
from pricing import price_line, coupon_discount


def checkout(shop):
    if not shop.cart:
        return ["error: cart empty"]
    out = []
    gross = 0
    subtotal = 0
    for line in shop.cart:
        p = price_line(shop, line)
        out.append(p.sku + " x" + str(p.qty) + " @ " + money(p.price) + " = " + money(p.gross))
        if p.free_amount > 0:
            out.append("  free x" + str(p.free_qty) + " -" + money(p.free_amount))
        if p.bulk_amount > 0:
            out.append("  bulk " + str(p.bulk_pct) + "% -" + money(p.bulk_amount))
        product = shop.catalog[p.sku]
        product.sold += p.qty
        product.net += p.net
        gross += p.gross
        subtotal += p.net
    out.append("subtotal " + money(subtotal))
    off = coupon_discount(shop, subtotal)
    if shop.active != "":
        out.append("coupon " + shop.active + " " + str(shop.coupons[shop.active]) + "% -" + money(off))
    total = subtotal - off
    out.append("total " + money(total))
    shop.sales += 1
    shop.revenue += total
    shop.discounts += gross - total
    shop.cart = []
    shop.active = ""
    return out


def report(shop):
    out = []
    for product in sorted_products(shop):
        if product.sold > 0:
            out.append(product.sku + " " + product.name + " sold " + str(product.sold) + " net " + money(product.net))
    out.append("sales " + str(shop.sales) + " revenue " + money(shop.revenue) + " discounts " + money(shop.discounts))
    return out
