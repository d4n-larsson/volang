from rooms import sorted_types, occupied, min_free
from bookings import held_nights
from billing import money


def avail_line(types, bookings, type_name, start, end):
    free = min_free(types, bookings, type_name, start, end)
    return ("avail " + type_name + " " + str(start) + "-" + str(end) + ": " + str(free)
            + " of " + str(types[type_name].count) + " free")


def occupancy_lines(types, bookings, day):
    used = 0
    total = 0
    rows = []
    for t in sorted_types(types):
        n = occupied(bookings, t.name, day)
        used += n
        total += t.count
        rows.append("  " + t.name + " " + str(n) + "/" + str(t.count))
    return ["day " + str(day) + ": " + str(used) + " of " + str(total) + " rooms occupied"] + rows


def report_lines(types, bookings):
    out = []
    revenue = 0
    refunds = 0
    any_cancelled = False
    for t in sorted_types(types):
        count = 0
        cancelled = 0
        held = 0
        earned = 0
        for k in bookings:
            b = bookings[k]
            if b.type_name == t.name:
                if b.cancelled:
                    cancelled += 1
                else:
                    count += 1
                held += held_nights(b)
                earned += b.total - b.refund
                refunds += b.refund
        revenue += earned
        any_cancelled = any_cancelled or cancelled > 0
        label = str(count) + " bookings"
        if cancelled > 0:
            label += " (" + str(cancelled) + " cancelled)"
        out.append(t.name + ": " + label + ", " + str(held) + " nights, revenue " + money(earned))
    total = "total revenue " + money(revenue)
    if any_cancelled:
        total += ", refunds " + money(refunds)
    out.append(total)
    return out
