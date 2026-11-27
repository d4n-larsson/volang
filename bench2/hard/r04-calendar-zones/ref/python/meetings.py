# Booking, moving and changing meetings.

import conflicts
import directory
import timefmt
from model import Meeting


def organizer_offset(cal, m):
    return cal.users[m.organizer].offset


def booked_text(cal, m):
    off = organizer_offset(cal, m)
    return ("booked " + m.meeting_id + " in " + m.room + ": " + timefmt.span(m.start + off, m.end + off)
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
    off = cal.users[organizer].offset
    start = start - off
    end = start + length
    e = conflicts.placement_error(cal, room, people, start, end, "", off)
    if e:
        return e
    m = Meeting(meeting_id, room, organizer, start, end, list(attendees))
    cal.meetings[meeting_id] = m
    return booked_text(cal, m)


def cancel(cal, meeting_id):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    del cal.meetings[meeting_id]
    return "cancelled " + meeting_id


def move(cal, meeting_id, start):
    if meeting_id not in cal.meetings:
        return "error: unknown meeting " + meeting_id
    m = cal.meetings[meeting_id]
    off = organizer_offset(cal, m)
    start = start - off
    end = start + m.length()
    e = conflicts.placement_error(cal, m.room, m.people(), start, end, meeting_id, off)
    if e:
        return e
    m.start = start
    m.end = end
    return "moved " + meeting_id + ": " + timefmt.span(m.start + off, m.end + off)


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
    e = conflicts.person_error(cal, name, m.start, m.end, meeting_id, organizer_offset(cal, m))
    if e:
        return e
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
