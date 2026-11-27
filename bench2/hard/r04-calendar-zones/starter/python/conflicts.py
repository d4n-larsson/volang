# Which meetings clash with a time slot.

import timefmt


def overlaps(start, end, other_start, other_end):
    return start < other_end and other_start < end


def first(meetings):
    """The meeting that starts first (ties: the smaller ID), or None."""
    best = None
    for m in meetings:
        if best is None or (m.start, m.meeting_id) < (best.start, best.meeting_id):
            best = m
    return best


def room_clash(cal, room, start, end, skip):
    found = [m for m in cal.meetings.values()
             if m.room == room and m.meeting_id != skip and overlaps(start, end, m.start, m.end)]
    return first(found)


def person_clash(cal, name, start, end, skip):
    found = [m for m in cal.meetings.values()
             if name in m.people() and m.meeting_id != skip and overlaps(start, end, m.start, m.end)]
    return first(found)


def clash_text(who, m):
    return "error: " + who + " busy: " + m.meeting_id + " " + timefmt.span(m.start, m.end)


def capacity_error(room, people_count):
    if people_count > room.capacity:
        return "error: room " + room.name + " holds " + str(room.capacity) + ", meeting has " + str(people_count)
    return ""


def placement_error(cal, room, people, start, end, skip):
    """The first clash for a meeting of `people` in `room` from start to end, or ""."""
    m = room_clash(cal, room, start, end, skip)
    if m is not None:
        return clash_text("room " + room, m)
    for name in people:
        m = person_clash(cal, name, start, end, skip)
        if m is not None:
            return clash_text(name, m)
    return ""
