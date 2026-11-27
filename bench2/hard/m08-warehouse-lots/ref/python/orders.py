from stock import Stock, fmt_picks


class Order:
    def __init__(self, oid, wh, sku):
        self.id, self.wh, self.sku = oid, wh, sku
        self.picks = []            # shipped picks: [lot, units, returned]
        self.backorder = 0
        self.holds = None          # [(entry, units)] while a reservation

    def returnable(self):
        return sum(q - r for _, q, r in self.picks)


class Desk:
    """Orders on top of the stock model; each command returns after appending its lines to out."""

    def __init__(self):
        self.stock = Stock()
        self.orders = {}
        self.order_list = []

    def check_wh(self, *whs):
        for w in whs:
            if w not in self.stock.warehouses:
                return f"no warehouse {w}"
        return None

    def check_item(self, sku):
        return None if sku in self.stock.items else f"no item {sku}"

    def new_order(self, oid, wh, sku):
        o = Order(oid, wh, sku)
        self.orders[oid] = o
        self.order_list.append(o)
        return o

    def ship_picks(self, o, picks):
        self.stock.ship(picks)
        o.picks += [[e.lot, q, 0] for e, q in picks]

    def settle(self, wh, sku, out):
        for o in self.order_list:
            if o.backorder > 0 and o.wh == wh and o.sku == sku:
                avail = self.stock.available(wh, sku)
                if avail == 0:
                    return
                n = min(o.backorder, avail)
                picks = self.stock.pick(wh, sku, n)
                self.ship_picks(o, picks)
                o.backorder -= n
                out.append(f"filled {o.id} {n}" + fmt_picks((e.lot, q) for e, q in picks))

    def do_warehouse(self, w, out):
        if w in self.stock.warehouses:
            return f"warehouse {w} exists"
        self.stock.warehouses.add(w)
        out.append(f"warehouse {w} added")

    def do_item(self, sku, out):
        if sku in self.stock.items:
            return f"item {sku} exists"
        self.stock.items.add(sku)
        out.append(f"item {sku} added")

    def do_receive(self, w, sku, qty, cost, expiry, out):
        e = self.check_wh(w) or self.check_item(sku)
        if e:
            return e
        if expiry <= self.stock.day:
            return "expired on arrival"
        lot = self.stock.receive(w, sku, qty, cost, expiry)
        out.append(f"lot L{lot.num} {w} {sku} {qty}")
        self.settle(w, sku, out)

    def do_ship(self, oid, w, sku, qty, out):
        if oid in self.orders:
            return f"order {oid} exists"
        e = self.check_wh(w) or self.check_item(sku)
        if e:
            return e
        o = self.new_order(oid, w, sku)
        n = min(qty, self.stock.available(w, sku))
        if n > 0:
            picks = self.stock.pick(w, sku, n)
            self.ship_picks(o, picks)
            out.append(f"shipped {oid} {n}" + fmt_picks((e.lot, q) for e, q in picks))
        if qty > n:
            o.backorder = qty - n
            out.append(f"backorder {oid} {qty - n}")

    def do_reserve(self, oid, w, sku, qty, out):
        if oid in self.orders:
            return f"order {oid} exists"
        e = self.check_wh(w) or self.check_item(sku)
        if e:
            return e
        avail = self.stock.available(w, sku)
        if avail < qty:
            return f"only {avail} available"
        o = self.new_order(oid, w, sku)
        o.holds = self.stock.pick(w, sku, qty)
        for e, q in o.holds:
            e.reserved += q
        out.append(f"reserved {oid} {qty}" + fmt_picks((e.lot, q) for e, q in o.holds))

    def reservation(self, oid):
        o = self.orders.get(oid)
        if o is None:
            return None, f"no order {oid}"
        if o.holds is None:
            return None, f"order {oid} is not reserved"
        return o, None

    def do_fulfil(self, oid, out):
        o, e = self.reservation(oid)
        if e:
            return e
        for en, q in o.holds:
            en.reserved -= q
        self.ship_picks(o, o.holds)
        out.append(f"shipped {oid} {sum(q for _, q in o.holds)}" + fmt_picks((e.lot, q) for e, q in o.holds))
        o.holds = None

    def do_release(self, oid, out):
        o, e = self.reservation(oid)
        if e:
            return e
        for en, q in o.holds:
            en.reserved -= q
        out.append(f"released {oid} {sum(q for _, q in o.holds)}")
        o.holds = None
        self.settle(o.wh, o.sku, out)

    def do_cancel(self, oid, out):
        o = self.orders.get(oid)
        if o is None:
            return f"no order {oid}"
        if o.backorder == 0:
            return f"order {oid} has no backorder"
        out.append(f"cancelled {oid} {o.backorder}")
        o.backorder = 0

    def do_return(self, oid, qty, out):
        o = self.orders.get(oid)
        if o is None:
            return f"no order {oid}"
        if qty > o.returnable():
            return f"order {oid} has {o.returnable()} returnable"
        left, restocked, quarantined = qty, 0, 0
        for p in reversed(o.picks):
            if left == 0:
                break
            take = min(left, p[1] - p[2])
            if take == 0:
                continue
            p[2] += take
            left -= take
            if self.stock.put_back(o.wh, p[0], take):
                restocked += take
            else:
                quarantined += take
        out.append(f"returned {oid} {qty} restocked {restocked} quarantined {quarantined}")
        if restocked > 0:
            self.settle(o.wh, o.sku, out)

    def do_transfer(self, src, dst, sku, qty, days, out):
        e = self.check_wh(src, dst)
        if e:
            return e
        if src == dst:
            return "same warehouse"
        e = self.check_item(sku)
        if e:
            return e
        avail = self.stock.available(src, sku)
        if avail < qty:
            return f"only {avail} available"
        t = self.stock.transfer(src, dst, sku, qty, days)
        out.append(f"transfer T{t.num} {qty} arrives {t.arrives}" + fmt_picks(t.picks))

    def do_day(self, d, out):
        st = self.stock
        if d < st.day:
            return f"day {st.day}"
        st.day = d
        arrived = st.arrivals(out)
        for e in st.expired_entries():
            st.write_off(e, out)
            for o in self.order_list:
                if o.holds is None:
                    continue
                lost = sum(q for en, q in o.holds if en is e)
                if lost == 0:
                    continue
                o.holds = [(en, q) for en, q in o.holds if en is not e]
                out.append(f"reservation {o.id} lost {lost}")
                if not o.holds:
                    o.holds = None
        for t in arrived:
            self.settle(t.dst, t.sku, out)
