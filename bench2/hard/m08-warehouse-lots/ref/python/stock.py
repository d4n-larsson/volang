class Lot:
    def __init__(self, num, sku, cost, expiry):
        self.num, self.sku, self.cost, self.expiry = num, sku, cost, expiry


class Entry:
    def __init__(self, wh, lot):
        self.wh, self.lot = wh, lot
        self.on_hand = 0
        self.reserved = 0


class Transfer:
    def __init__(self, num, src, dst, sku, picks, arrives):
        self.num, self.src, self.dst, self.sku, self.picks, self.arrives = num, src, dst, sku, picks, arrives
        self.delivered = False

    def qty(self):
        return sum(q for _, q in self.picks)


def fmt_picks(picks):
    return "".join(f" L{lot.num}x{q}" for lot, q in picks)


class Stock:
    def __init__(self):
        self.warehouses = set()
        self.items = set()
        self.lots = []
        self.entries = {}          # (warehouse, lot number) -> Entry
        self.quarantine = {}       # (warehouse, sku) -> units
        self.transfers = []
        self.day = 0
        self.shipped = 0
        self.written_off = 0
        self.quarantine_value = 0

    def entry(self, wh, lot):
        key = (wh, lot.num)
        if key not in self.entries:
            self.entries[key] = Entry(wh, lot)
        return self.entries[key]

    def fefo(self, wh, sku):
        es = [e for e in self.entries.values() if e.wh == wh and e.lot.sku == sku]
        return sorted(es, key=lambda e: (e.lot.expiry, e.lot.num))

    def available(self, wh, sku):
        return sum(e.on_hand - e.reserved for e in self.fefo(wh, sku))

    def pick(self, wh, sku, q):
        """[(entry, units)] in FEFO order; nothing is changed."""
        out = []
        for e in self.fefo(wh, sku):
            take = min(q, e.on_hand - e.reserved)
            if take > 0:
                out.append((e, take))
                q -= take
            if q == 0:
                break
        return out

    def ship(self, picks):
        for e, q in picks:
            e.on_hand -= q
            self.shipped += q * e.lot.cost

    def receive(self, wh, sku, qty, cost, expiry):
        lot = Lot(len(self.lots) + 1, sku, cost, expiry)
        self.lots.append(lot)
        self.entry(wh, lot).on_hand += qty
        return lot

    def transfer(self, src, dst, sku, qty, days):
        picks = self.pick(src, sku, qty)
        for e, q in picks:
            e.on_hand -= q
        t = Transfer(len(self.transfers) + 1, src, dst, sku, [(e.lot, q) for e, q in picks], self.day + days)
        self.transfers.append(t)
        return t

    def arrivals(self, out):
        due = sorted((t for t in self.transfers if not t.delivered and t.arrives <= self.day),
                     key=lambda t: (t.arrives, t.num))
        for t in due:
            t.delivered = True
            for lot, q in t.picks:
                self.entry(t.dst, lot).on_hand += q
            out.append(f"arrived T{t.num} {t.dst} {t.sku} {t.qty()}")
        return due

    def expired_entries(self):
        es = [e for e in self.entries.values() if e.on_hand > 0 and e.lot.expiry <= self.day]
        return sorted(es, key=lambda e: (e.lot.num, e.wh.encode()))

    def write_off(self, e, out):
        out.append(f"expired L{e.lot.num} {e.wh} {e.on_hand}")
        self.written_off += e.on_hand * e.lot.cost
        e.on_hand = e.reserved = 0

    def put_back(self, wh, lot, q):
        """Returned units: True if restocked, False if quarantined."""
        if lot.expiry > self.day:
            self.entry(wh, lot).on_hand += q
            self.shipped -= q * lot.cost
            return True
        key = (wh, lot.sku)
        self.quarantine[key] = self.quarantine.get(key, 0) + q
        self.shipped -= q * lot.cost
        self.quarantine_value += q * lot.cost
        return False
