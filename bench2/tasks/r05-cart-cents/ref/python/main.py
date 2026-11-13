import sys

from shop import Shop
from money import is_number, parse_amount
from cart import add_product, set_price, add_to_cart, remove_from_cart, show_cart
from pricing import set_promo, set_bulk, define_coupon, use_coupon
from checkout import checkout, report


def positive(s):
    return is_number(s) and int(s) >= 1


def percent(s):
    return positive(s) and int(s) <= 100


def run(words, shop):
    cmd = words[0]
    args = words[1:]
    bad = ["error: bad command"]
    if cmd == "product":
        if len(args) != 3 or parse_amount(args[2]) is None:
            return bad
        return [add_product(shop, args[0], args[1], parse_amount(args[2]))]
    if cmd == "price":
        if len(args) != 2 or parse_amount(args[1]) is None:
            return bad
        return [set_price(shop, args[0], parse_amount(args[1]))]
    if cmd == "add":
        if len(args) != 2 or not positive(args[1]):
            return bad
        return [add_to_cart(shop, args[0], int(args[1]))]
    if cmd == "remove":
        if len(args) != 2 or not positive(args[1]):
            return bad
        return [remove_from_cart(shop, args[0], int(args[1]))]
    if cmd == "promo":
        if len(args) != 2 or not positive(args[1]):
            return bad
        return [set_promo(shop, args[0], int(args[1]))]
    if cmd == "bulk":
        if len(args) != 3 or not positive(args[1]) or not percent(args[2]):
            return bad
        return [set_bulk(shop, args[0], int(args[1]), int(args[2]))]
    if cmd == "coupon":
        if len(args) != 2 or not percent(args[1]):
            return bad
        return [define_coupon(shop, args[0], int(args[1]))]
    if cmd == "use":
        if len(args) != 1:
            return bad
        return [use_coupon(shop, args[0])]
    if cmd == "cart":
        if len(args) != 0:
            return bad
        return show_cart(shop)
    if cmd == "checkout":
        if len(args) != 0:
            return bad
        return checkout(shop)
    if cmd == "report":
        if len(args) != 0:
            return bad
        return report(shop)
    return ["error: unknown command " + cmd]


def main():
    shop = Shop()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, shop):
            print(out)


main()
