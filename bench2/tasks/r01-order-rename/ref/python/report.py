from catalog import sorted_products
from orders import units


def money(cents):
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(cents // 100) + "." + frac


def placed_line(order):
    text = "order " + order.order_id + " placed: " + str(units(order)) + " units, total " + money(order.total)
    if order.discount > 0:
        text += " (discount " + money(order.discount) + ")"
    return text


def show_order(order):
    status = "cancelled" if order.cancelled else "active"
    out = ["order " + order.order_id + " (" + status + ")"]
    for line in order.lines:
        out.append("  " + line.sku + " x" + str(line.qty) + " @ " + money(line.price))
    out.append("  total " + money(order.total))
    return out


def full_report(catalog, orders):
    out = []
    for product in sorted_products(catalog):
        out.append("product " + product.sku + " " + product.name + " price " + money(product.price)
                   + " stock " + str(product.stock) + " sold " + str(product.sold))
    revenue = 0
    active = 0
    for order_id in orders:
        order = orders[order_id]
        if not order.cancelled:
            revenue += order.total
            active += 1
    out.append("revenue " + money(revenue) + " over " + str(active) + " orders")
    return out
