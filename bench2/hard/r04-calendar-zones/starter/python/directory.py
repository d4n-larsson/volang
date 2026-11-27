# Users and rooms. They share one namespace: a name is a user or a room, never both.

from model import User, Room


def name_taken(cal, name):
    return name in cal.users or name in cal.rooms


def add_user(cal, name):
    if name_taken(cal, name):
        return "error: name " + name + " taken"
    cal.users[name] = User(name)
    return "user " + name + " added"


def add_room(cal, name, capacity):
    if name_taken(cal, name):
        return "error: name " + name + " taken"
    cal.rooms[name] = Room(name, capacity)
    return "room " + name + " added, capacity " + str(capacity)


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
