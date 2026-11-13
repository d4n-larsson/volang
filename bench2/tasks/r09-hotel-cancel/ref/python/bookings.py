from model import Booking
from rooms import first_full_day
from billing import stay_discount, money, refund_for


def held_nights(b):
    return b.held_until - b.start


def book(types, bookings, booking_id, type_name, guest, start, end):
    if booking_id in bookings:
        return "error: booking " + booking_id + " exists"
    if type_name not in types:
        return "error: unknown type " + type_name
    if start >= end:
        return "error: bad dates"
    full = first_full_day(types, bookings, type_name, start, end)
    if full >= 0:
        return "error: no " + type_name + " room free on day " + str(full)
    rate = types[type_name].rate
    subtotal = (end - start) * rate
    b = Booking(booking_id, type_name, guest, start, end, rate, subtotal, stay_discount(end - start, subtotal))
    bookings[booking_id] = b
    return "booked " + booking_id + ": " + type_name + " " + str(b.end - b.start) + " nights, total " + money(b.total)


def cancel(bookings, booking_id, day):
    if booking_id not in bookings:
        return "error: unknown booking " + booking_id
    b = bookings[booking_id]
    if b.cancelled:
        return "error: booking " + booking_id + " already cancelled"
    if day >= b.end:
        return "error: booking " + booking_id + " already ended"
    b.refund = refund_for(b, day)
    b.cancelled = True
    b.cancel_day = day
    b.held_until = max(b.start, day)
    return "cancelled " + booking_id + ": refund " + money(b.refund)
