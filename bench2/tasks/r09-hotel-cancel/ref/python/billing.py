LONG_STAY_NIGHTS = 7
LONG_STAY_PERCENT = 10


def money(cents):
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(cents // 100) + "." + frac


def stay_discount(nights, subtotal):
    if nights >= LONG_STAY_NIGHTS:
        return subtotal * LONG_STAY_PERCENT // 100
    return 0


def refund_for(b, day):
    """Full refund 7+ days before check-in, half (rounded down) 1-6 days before, none from check-in."""
    if b.start - day >= 7:
        return b.total
    if day < b.start:
        return b.total // 2
    return 0


def invoice_lines(b):
    nights = b.end - b.start
    out = ["invoice " + b.booking_id + " for " + b.guest]
    out.append("  " + b.type_name + " " + str(nights) + " nights x " + money(b.rate) + " = " + money(b.subtotal))
    if b.discount > 0:
        out.append("  long stay discount " + money(b.discount))
    out.append("  total " + money(b.total))
    if b.cancelled:
        out.append("  cancelled on day " + str(b.cancel_day) + ", refund " + money(b.refund))
        out.append("  net " + money(b.total - b.refund))
    return out
