# Which meetings clash with a time slot.

import timefmt
from model import MINUTES_PER_DAY


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


def clash_text(who, m, offset):
    return "error: " + who + " busy: " + m.meeting_id + " " + timefmt.span(m.start + offset, m.end + offset)


def hours_error(user, start, end):
    """Whether a meeting from start to end (UTC) lies within the user's working hours of one local day."""
    if user.hours is None:
        return ""
    local_start = start + user.offset
    base = timefmt.day_of(local_start) * MINUTES_PER_DAY
    if local_start < base + user.hours[0] or end + user.offset > base + user.hours[1]:
        return ("error: " + user.name + " works " + timefmt.clock(user.hours[0]) + "-"
                + timefmt.clock(user.hours[1]))
    return ""


def person_error(cal, name, start, end, skip, offset):
    m = person_clash(cal, name, start, end, skip)
    if m is not None:
        return clash_text(name, m, offset)
    return hours_error(cal.users[name], start, end)


def capacity_error(room, people_count):
    if people_count > room.capacity:
        return "error: room " + room.name + " holds " + str(room.capacity) + ", meeting has " + str(people_count)
    return ""


def placement_error(cal, room, people, start, end, skip, offset):
    """The first clash for a meeting of `people` in `room` from start to end, or "".
    Times in messages are shown with `offset` (the organizer's zone)."""
    m = room_clash(cal, room, start, end, skip)
    if m is not None:
        return clash_text("room " + room, m, offset)
    for name in people:
        e = person_error(cal, name, start, end, skip, offset)
        if e:
            return e
    return ""
