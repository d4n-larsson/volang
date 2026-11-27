# Users and rooms. They share one namespace: a name is a user or a room, never both.

import timefmt
from model import User, Room


def name_taken(cal, name):
    return name in cal.users or name in cal.rooms


def add_user(cal, name, offset):
    if name_taken(cal, name):
        return "error: name " + name + " taken"
    cal.users[name] = User(name, offset)
    return "user " + name + " added"


def add_room(cal, name, capacity, offset):
    if name_taken(cal, name):
        return "error: name " + name + " taken"
    cal.rooms[name] = Room(name, capacity, offset)
    return "room " + name + " added, capacity " + str(capacity)


def set_zone(cal, name, offset):
    if name in cal.users:
        cal.users[name].offset = offset
    elif name in cal.rooms:
        cal.rooms[name].offset = offset
    else:
        return "error: unknown name " + name
    return "zone " + name + " " + timefmt.zone(offset)


def set_hours(cal, name, hours):
    if name not in cal.users:
        return "error: unknown user " + name
    cal.users[name].hours = hours
    if hours is None:
        return "hours " + name + " none"
    return "hours " + name + " " + timefmt.clock(hours[0]) + "-" + timefmt.clock(hours[1])


def sorted_rooms(cal):
    return [cal.rooms[n] for n in sorted(cal.rooms)]


def people_error(cal, organizer, attendees):
    """The first problem with a meeting's people, or ""."""
    if organizer not in cal.users:
        return "error: unknown user " + organizer
    seen = [organizer]
    for a in attendees:
        if a not in cal.users:
            return "error: unknown user " + a
        if a in seen:
            return "error: " + a + " listed twice"
        seen.append(a)
    return ""
