from model import RoomType


def add_type(types, name, rate, count):
    if name in types:
        return "error: type " + name + " exists"
    types[name] = RoomType(name, rate, count)
    return "type " + name + " added"


def set_rate(types, name, rate, money):
    if name not in types:
        return "error: unknown type " + name
    types[name].rate = rate
    return "type " + name + " rate " + money(rate)


def sorted_types(types):
    return [types[k] for k in sorted(types)]


def occupied(bookings, type_name, day):
    """How many rooms of the type are taken on the night of `day`."""
    n = 0
    for k in bookings:
        b = bookings[k]
        if b.type_name == type_name and b.start <= day and day < b.end:
            n += 1
    return n


def first_full_day(types, bookings, type_name, start, end):
    """The first night in [start, end) with no free room of the type, or -1."""
    count = types[type_name].count
    for day in range(start, end):
        if occupied(bookings, type_name, day) >= count:
            return day
    return -1


def min_free(types, bookings, type_name, start, end):
    t = types[type_name]
    best = t.count
    for day in range(start, end):
        best = min(best, t.count - occupied(bookings, type_name, day))
    return best
