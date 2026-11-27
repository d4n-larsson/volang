from parsing import money


def byte_sorted(xs):
    return sorted(xs, key=lambda s: s.encode())


def stock_report(st, out):
    out.append(f"stock day {st.day}")
    for w in byte_sorted(st.warehouses):
        for sku in byte_sorted(st.items):
            for e in st.fefo(w, sku):
                if e.on_hand > 0:
                    out.append(f"{w} {sku} L{e.lot.num} on hand {e.on_hand} reserved {e.reserved} expires {e.lot.expiry}")
            q = st.quarantine.get((w, sku), 0)
            if q > 0:
                out.append(f"{w} {sku} quarantine {q}")
    for t in st.transfers:
        if not t.delivered:
            out.append(f"transit T{t.num} {t.src} {t.dst} {t.sku} {t.qty()} arrives {t.arrives}")


def value_report(st, out):
    on_hand = sum(e.on_hand * e.lot.cost for e in st.entries.values())
    transit = sum(q * lot.cost for t in st.transfers if not t.delivered for lot, q in t.picks)
    out.append(f"value on hand {money(on_hand)} in transit {money(transit)} quarantine {money(st.quarantine_value)} "
               f"shipped {money(st.shipped)} written off {money(st.written_off)}")


def backorder_report(desk, out):
    open_ = [o for o in desk.order_list if o.backorder > 0]
    out.append(f"backorders {len(open_)}")
    for o in open_:
        out.append(f"{o.id} {o.wh} {o.sku} {o.backorder}")
