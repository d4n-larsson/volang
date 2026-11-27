# Read-only views of the calendar.

import conflicts
import directory
import timefmt
from model import MINUTES_PER_DAY

WORK_START = 9 * 60
WORK_END = 17 * 60


def by_start(meetings):
    return sorted(meetings, key=lambda m: (m.start, m.meeting_id))


def meetings_of(cal, name):
    return by_start([m for m in cal.meetings.values() if name in m.people()])


def meetings_in(cal, room):
    return by_start([m for m in cal.meetings.values() if m.room == room])


def day_bounds(day):
    return day * MINUTES_PER_DAY, (day + 1) * MINUTES_PER_DAY


def agenda(cal, name, day):
    if name not in cal.users:
        return ["error: unknown user " + name]
    lo, hi = day_bounds(day)
    out = ["agenda " + name + " day " + str(day) + ":"]
    for m in meetings_of(cal, name):
        if conflicts.overlaps(m.start, m.end, lo, hi):
            role = "organizer" if m.organizer == name else "attendee"
            out.append("  " + timefmt.span(m.start, m.end) + " " + m.meeting_id + " in " + m.room + " (" + role + ")")
    if len(out) == 1:
        out.append("  nothing")
    return out


def rooms_on(cal, day):
    lo, hi = day_bounds(day)
    out = ["rooms day " + str(day) + ":"]
    for r in directory.sorted_rooms(cal):
        items = [m.meeting_id + " " + timefmt.span(m.start, m.end)
                 for m in meetings_in(cal, r.name) if conflicts.overlaps(m.start, m.end, lo, hi)]
        out.append("  " + r.name + ": " + timefmt.joined(items, "free"))
    if len(out) == 1:
        out.append("  no rooms")
    return out


def free_slots(cal, room, day):
    if room not in cal.rooms:
        return "error: unknown room " + room
    base = day * MINUTES_PER_DAY
    lo, hi = base + WORK_START, base + WORK_END
    slots = []
    at = lo
    for m in meetings_in(cal, room):
        if not conflicts.overlaps(m.start, m.end, lo, hi):
            continue
        if m.start > at:
            slots.append(timefmt.clock(at) + "-" + timefmt.clock(m.start))
        at = max(at, m.end)
    if at < hi:
        slots.append(timefmt.clock(at) + "-" + timefmt.clock(hi))
    return "free " + room + " day " + str(day) + ": " + timefmt.joined(slots, "none")


def busy(cal, name, day):
    """Meetings of a user and their minutes: all of them (day < 0) or the parts on one day."""
    if name not in cal.users:
        return "error: unknown user " + name
    n = 0
    total = 0
    if day < 0:
        for m in meetings_of(cal, name):
            n += 1
            total += m.length()
        return name + ": " + timefmt.count(n, "meeting", "meetings") + ", " + timefmt.duration(total)
    lo, hi = day_bounds(day)
    for m in meetings_of(cal, name):
        if conflicts.overlaps(m.start, m.end, lo, hi):
            n += 1
            total += min(m.end, hi) - max(m.start, lo)
    return (name + " day " + str(day) + ": " + timefmt.count(n, "meeting", "meetings") + ", "
            + timefmt.duration(total))


def who(cal, meeting_id):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    m = cal.meetings[meeting_id]
    return (meeting_id + ": " + m.room + " " + timefmt.span(m.start, m.end) + ", organizer " + m.organizer
            + ", attendees " + timefmt.joined(m.attendees, "none"))


def listing(cal):
    out = []
    for m in by_start(list(cal.meetings.values())):
        out.append(m.meeting_id + " " + timefmt.span(m.start, m.end) + " " + m.room + " " + m.organizer
                   + " +" + str(len(m.attendees)))
    if len(out) == 0:
        out.append("no meetings")
    return out
