class RoomType:
    def __init__(self, name, rate, count):
        self.name = name
        self.rate = rate
        self.count = count


class Booking:
    """Nights start..end-1: check-in on day `start`, check-out on day `end`. Amounts in cents."""

    def __init__(self, booking_id, type_name, guest, start, end, rate, subtotal, discount):
        self.booking_id = booking_id
        self.type_name = type_name
        self.guest = guest
        self.start = start
        self.end = end
        self.rate = rate
        self.subtotal = subtotal
        self.discount = discount
        self.total = subtotal - discount
