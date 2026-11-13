from clock import duration, span


class Event:
    """start and end are minutes since midnight."""

    def __init__(self, event_id, room, start, end):
        self.event_id = event_id
        self.room = room
        self.start = start
        self.end = end


class Slot:
    def __init__(self, start, end):
        self.start = start
        self.end = end


def overlaps(a_start, a_end, b_start, b_end):
    return a_start < b_end and b_start < a_end


def room_events(events, room):
    """The room's events sorted by start time."""
    evs = [events[k] for k in events if events[k].room == room]
    evs.sort(key=lambda e: e.start)
    return evs


def add_event(events, event_id, room, start, end):
    if event_id in events:
        return "error: event " + event_id + " exists"
    if start >= end:
        return "error: empty interval"
    for e in room_events(events, room):
        if overlaps(start, end, e.start, e.end):
            return "error: " + event_id + " conflicts with " + e.event_id
    events[event_id] = Event(event_id, room, start, end)
    return "added " + event_id + " in " + room + " " + span(start, end)


def remove_event(events, event_id):
    if event_id not in events:
        return "error: unknown event " + event_id
    del events[event_id]
    return "removed " + event_id


def event_at(events, room, t):
    for e in room_events(events, room):
        if e.start <= t and t < e.end:
            return e
    return None


def free_slots(events, room, lo, hi, min_len):
    """Gaps inside [lo, hi) not covered by the room's events, at least min_len minutes long."""
    slots = []
    cursor = lo
    for e in room_events(events, room):
        if e.end <= cursor:
            continue
        if e.start >= hi:
            break
        if e.start > cursor and duration(cursor, e.start) >= min_len:
            slots.append(Slot(cursor, e.start))
        cursor = max(cursor, e.end)
    if cursor < hi and duration(cursor, hi) >= min_len:
        slots.append(Slot(cursor, hi))
    return slots


def booked_minutes(evs):
    return sum(duration(e.start, e.end) for e in evs)
