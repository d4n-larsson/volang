"""Command parsing and money formatting."""
import re

ID = re.compile(r"[A-Za-z0-9]{1,8}")
TRADER = re.compile(r"[a-z]{1,10}")
SYMBOL = re.compile(r"[A-Z]{1,5}")
QTY = re.compile(r"[1-9][0-9]{0,5}")
PRICE = re.compile(r"(0|[1-9][0-9]{0,5})\.([0-9]{2})")


def money(c):
    sign = "-" if c < 0 else ""
    c = abs(c)
    return f"{sign}{c // 100}.{c % 100:02d}"


def price(s):
    m = PRICE.fullmatch(s)
    if not m:
        return None
    v = int(m.group(1)) * 100 + int(m.group(2))
    return v if v > 0 else None


def ok(rx, s):
    return rx.fullmatch(s) is not None


def parse(line):
    """None for an invalid line, otherwise a dict describing the command."""
    f = line.split()
    if not f:
        return {"cmd": "blank"}
    cmd, n = f[0], len(f)
    if cmd == "limit" and n in (7, 8):
        tif = f[7] if n == 8 else "gtc"
        p = price(f[6])
        if not (order_fields(f) and ok(QTY, f[5]) and p and tif in ("gtc", "ioc", "fok")):
            return None
        return {"cmd": "limit", "id": f[1], "trader": f[2], "side": f[3], "symbol": f[4],
                "qty": int(f[5]), "price": p, "tif": tif}
    if cmd == "market" and n == 6:
        if not (order_fields(f) and ok(QTY, f[5])):
            return None
        return {"cmd": "market", "id": f[1], "trader": f[2], "side": f[3], "symbol": f[4],
                "qty": int(f[5]), "price": None, "tif": "market"}
    if cmd == "stop" and n == 7:
        p = price(f[6])
        if not (order_fields(f) and ok(QTY, f[5]) and p):
            return None
        return {"cmd": "stop", "id": f[1], "trader": f[2], "side": f[3], "symbol": f[4],
                "qty": int(f[5]), "price": p, "tif": "stop"}
    if cmd == "cancel" and n == 2 and ok(ID, f[1]):
        return {"cmd": "cancel", "id": f[1]}
    if cmd == "modify" and n == 4:
        p = price(f[3])
        if ok(ID, f[1]) and ok(QTY, f[2]) and p:
            return {"cmd": "modify", "id": f[1], "qty": int(f[2]), "price": p}
        return None
    if cmd == "book" and n == 2 and ok(SYMBOL, f[1]):
        return {"cmd": "book", "symbol": f[1]}
    return None


def order_fields(f):
    return ok(ID, f[1]) and ok(TRADER, f[2]) and f[3] in ("buy", "sell") and ok(SYMBOL, f[4])
