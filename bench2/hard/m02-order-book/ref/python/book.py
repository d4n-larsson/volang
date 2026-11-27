"""Order books, matching and pending stop orders."""
from parse import money


class Order:
    def __init__(self, oid, trader, side, symbol, qty, price):
        self.id, self.trader, self.side, self.symbol = oid, trader, side, symbol
        self.qty, self.price, self.prio = qty, price, 0


class Engine:
    def __init__(self, accounts, out):
        self.acc, self.out = accounts, out
        self.resting = []          # all resting orders, every symbol
        self.stops = []            # pending stops in acceptance order
        self.last = {}
        self.counter = 0

    def opposite(self, o, limit):
        """Resting orders the incoming order accepts, best first."""
        side = "sell" if o.side == "buy" else "buy"
        rs = [r for r in self.resting if r.symbol == o.symbol and r.side == side]
        if limit is not None:
            rs = [r for r in rs if (r.price <= limit if o.side == "buy" else r.price >= limit)]
        if o.side == "buy":
            rs.sort(key=lambda r: (r.price, r.prio))
        else:
            rs.sort(key=lambda r: (-r.price, r.prio))
        return rs

    def match(self, o, limit):
        while o.qty > 0:
            rs = self.opposite(o, limit)
            if not rs:
                break
            r = rs[0]
            if r.trader == o.trader:
                self.resting.remove(r)
                self.out.append(f"stp {r.id} {r.qty}")
                continue
            t = min(o.qty, r.qty)
            buy, sell = (o, r) if o.side == "buy" else (r, o)
            self.out.append(f"trade {o.symbol} {t} {money(r.price)} {buy.id} {sell.id}")
            o.qty -= t
            r.qty -= t
            if r.qty == 0:
                self.resting.remove(r)
            self.last[o.symbol] = r.price
            self.acc.trade(buy.trader, sell.trader, o.symbol, t, r.price)

    def place(self, o):
        self.counter += 1
        o.prio = self.counter
        self.resting.append(o)
        self.out.append(f"rest {o.id} {o.qty} {money(o.price)}")

    def in_band(self, symbol, p):
        if symbol not in self.last:
            return True
        last = self.last[symbol]
        return 10 * p >= 9 * last and 10 * p <= 11 * last

    def limit(self, c):
        o = Order(c["id"], c["trader"], c["side"], c["symbol"], c["qty"], c["price"])
        if not self.in_band(o.symbol, o.price):
            return f"price band"
        if c["tif"] == "fok":
            avail = sum(r.qty for r in self.opposite(o, o.price) if r.trader != o.trader)
            if avail < o.qty:
                return "not filled"
        self.acc.open(o.trader)
        self.enter(o, c["tif"])
        return None

    def enter(self, o, tif):
        self.match(o, o.price)
        if o.qty == 0:
            self.out.append(f"filled {o.id}")
        elif tif == "gtc":
            self.place(o)
        else:
            self.out.append(f"expired {o.id} {o.qty}")

    def market(self, c):
        o = Order(c["id"], c["trader"], c["side"], c["symbol"], c["qty"], None)
        if not any(r.trader != o.trader for r in self.opposite(o, None)):
            return "no liquidity"
        self.acc.open(o.trader)
        self.run_market(o)
        return None

    def run_market(self, o):
        self.match(o, None)
        if o.qty == 0:
            self.out.append(f"filled {o.id}")
        else:
            self.out.append(f"expired {o.id} {o.qty}")

    def stop(self, c):
        self.acc.open(c["trader"])
        self.stops.append(Order(c["id"], c["trader"], c["side"], c["symbol"], c["qty"], c["price"]))
        self.out.append(f"pending {c['id']}")

    def find_resting(self, oid):
        for r in self.resting:
            if r.id == oid:
                return r
        return None

    def cancel(self, oid):
        r = self.find_resting(oid)
        if r:
            self.resting.remove(r)
        else:
            r = next((s for s in self.stops if s.id == oid), None)
            if not r:
                return "unknown order"
            self.stops.remove(r)
        self.out.append(f"cancelled {oid} {r.qty}")
        return None

    def modify(self, c):
        r = self.find_resting(c["id"])
        if not r:
            return "unknown order", None
        if c["price"] != r.price and not self.in_band(r.symbol, c["price"]):
            return "price band", None
        self.out.append(f"modified {r.id} {c['qty']} {money(c['price'])}")
        if c["price"] == r.price and c["qty"] <= r.qty:
            r.qty = c["qty"]
        else:
            self.resting.remove(r)
            r.qty, r.price = c["qty"], c["price"]
            self.enter(r, "gtc")
        return None, r.symbol

    def triggers(self, symbol):
        while symbol in self.last:
            last = self.last[symbol]
            s = next((s for s in self.stops if s.symbol == symbol and
                      (last >= s.price if s.side == "buy" else last <= s.price)), None)
            if s is None:
                return
            self.stops.remove(s)
            self.out.append(f"trigger {s.id}")
            self.run_market(s)

    def book(self, symbol):
        lines = [f"book {symbol} last " + (money(self.last[symbol]) if symbol in self.last else "-")]
        for side in ("sell", "buy"):
            levels = {}
            for r in self.resting:
                if r.symbol == symbol and r.side == side:
                    q, n = levels.get(r.price, (0, 0))
                    levels[r.price] = (q + r.qty, n + 1)
            for p in sorted(levels, reverse=True):
                name = "ask" if side == "sell" else "bid"
                lines.append(f"  {name} {money(p)} {levels[p][0]} {levels[p][1]}")
        if len(lines) == 1:
            lines.append("  empty")
        return lines
