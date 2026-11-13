from model import Booking
from rooms import first_full_day
from billing import stay_discount, money


def nights(b):
    return b.end - b.start


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
    return "booked " + booking_id + ": " + type_name + " " + str(nights(b)) + " nights, total " + money(b.total)
