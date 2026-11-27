# Booking, moving and changing meetings.

import conflicts
import directory
import timefmt
from model import Meeting


def booked_text(m):
    return ("booked " + m.meeting_id + " in " + m.room + ": " + timefmt.span(m.start, m.end)
            + " (" + timefmt.count(len(m.people()), "person", "people") + ")")


def book(cal, meeting_id, room, organizer, start, length, attendees):
    if meeting_id in cal.meetings:
        return "error: meeting " + meeting_id + " exists"
    if room not in cal.rooms:
        return "error: unknown room " + room
    e = directory.people_error(cal, organizer, attendees)
    if e:
        return e
    people = [organizer] + attendees
    e = conflicts.capacity_error(cal.rooms[room], len(people))
    if e:
        return e
    end = start + length
    e = conflicts.placement_error(cal, room, people, start, end, "")
    if e:
        return e
    m = Meeting(meeting_id, room, organizer, start, end, list(attendees))
    cal.meetings[meeting_id] = m
    return booked_text(m)


def cancel(cal, meeting_id):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    del cal.meetings[meeting_id]
    return "cancelled " + meeting_id


def move(cal, meeting_id, start):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    m = cal.meetings[meeting_id]
    end = start + m.length()
    e = conflicts.placement_error(cal, m.room, m.people(), start, end, meeting_id)
    if e:
        return e
    m.start = start
    m.end = end
    return "moved " + meeting_id + ": " + timefmt.span(m.start, m.end)


def invite(cal, meeting_id, name):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    if name not in cal.users:
        return "error: unknown user " + name
    m = cal.meetings[meeting_id]
    if name in m.people():
        return "error: " + name + " already in " + meeting_id
    e = conflicts.capacity_error(cal.rooms[m.room], len(m.people()) + 1)
    if e:
        return e
    other = conflicts.person_clash(cal, name, m.start, m.end, meeting_id)
    if other is not None:
        return conflicts.clash_text(name, other)
    m.attendees.append(name)
    return name + " invited to " + meeting_id


def leave(cal, meeting_id, name):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    m = cal.meetings[meeting_id]
    if name == m.organizer:
        return "error: " + name + " organizes " + meeting_id
    if name not in m.attendees:
        return "error: " + name + " not in " + meeting_id
    m.attendees.remove(name)
    return name + " left " + meeting_id
