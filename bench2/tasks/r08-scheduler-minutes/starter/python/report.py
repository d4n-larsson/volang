from clock import duration, span
from schedule import room_events, event_at, free_slots, booked_minutes


def agenda_lines(events, room):
    evs = room_events(events, room)
    out = ["agenda " + room + ": " + str(len(evs)) + " events"]
    for e in evs:
        out.append("  " + span(e.start, e.end) + " " + e.event_id + " (" + str(duration(e.start, e.end)) + " min)")
    return out


def who_line(events, room, t):
    e = event_at(events, room, t)
    if e is None:
        return room + " at " + t + ": free"
    return room + " at " + t + ": " + e.event_id


def free_lines(events, room, lo, hi, min_len):
    out = ["free " + room + " " + span(lo, hi) + ":"]
    for s in free_slots(events, room, lo, hi, min_len):
        out.append("  " + span(s.start, s.end) + " (" + str(duration(s.start, s.end)) + " min)")
    if len(out) == 1:
        out.append("  none")
    return out


def summary_lines(events):
    rooms = sorted(set(events[k].room for k in events))
    out = []
    total = 0
    for room in rooms:
        evs = room_events(events, room)
        booked = booked_minutes(evs)
        total += booked
        out.append(room + ": " + str(len(evs)) + " events, " + str(booked) + " min booked, first "
                   + evs[0].start + ", last " + evs[-1].end)
    out.append("total: " + str(len(events)) + " events, " + str(total) + " min booked")
    return out
