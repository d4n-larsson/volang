# One function per command: checks the arguments, then calls the module that does the work.
# A command with the wrong arguments prints `error: bad command` and changes nothing.

import directory
import meetings
import parse
import views

BAD = ["error: bad command"]


def cmd_user(cal, args):
    if len(args) != 1:
        return BAD
    return [directory.add_user(cal, args[0])]


def cmd_room(cal, args):
    if len(args) != 2 or parse.parse_capacity(args[1]) < 0:
        return BAD
    return [directory.add_room(cal, args[0], parse.parse_capacity(args[1]))]


def cmd_book(cal, args):
    # book ID ROOM ORGANIZER DAY HH:MM MINUTES [ATTENDEE...]
    if len(args) < 6:
        return BAD
    start = parse.parse_moment(args[3], args[4])
    length = parse.parse_length(args[5])
    if start < 0 or length < 0:
        return BAD
    return [meetings.book(cal, args[0], args[1], args[2], start, length, args[6:])]


def cmd_cancel(cal, args):
    if len(args) != 1:
        return BAD
    return [meetings.cancel(cal, args[0])]


def cmd_move(cal, args):
    if len(args) != 3:
        return BAD
    start = parse.parse_moment(args[1], args[2])
    if start < 0:
        return BAD
    return [meetings.move(cal, args[0], start)]


def cmd_invite(cal, args):
    if len(args) != 2:
        return BAD
    return [meetings.invite(cal, args[0], args[1])]


def cmd_leave(cal, args):
    if len(args) != 2:
        return BAD
    return [meetings.leave(cal, args[0], args[1])]


def cmd_agenda(cal, args):
    if len(args) != 2 or not parse.is_number(args[1]):
        return BAD
    return views.agenda(cal, args[0], parse.number(args[1]))


def cmd_rooms(cal, args):
    if len(args) != 1 or not parse.is_number(args[0]):
        return BAD
    return views.rooms_on(cal, parse.number(args[0]))


def cmd_free(cal, args):
    if len(args) != 2 or not parse.is_number(args[1]):
        return BAD
    return [views.free_slots(cal, args[0], parse.number(args[1]))]


def cmd_busy(cal, args):
    if len(args) == 1:
        return [views.busy(cal, args[0], -1)]
    if len(args) == 2 and parse.is_number(args[1]):
        return [views.busy(cal, args[0], parse.number(args[1]))]
    return BAD


def cmd_who(cal, args):
    if len(args) != 1:
        return BAD
    return [views.who(cal, args[0])]


def cmd_list(cal, args):
    if len(args) != 0:
        return BAD
    return views.listing(cal)


COMMANDS = {
    "user": cmd_user,
    "room": cmd_room,
    "book": cmd_book,
    "cancel": cmd_cancel,
    "move": cmd_move,
    "invite": cmd_invite,
    "leave": cmd_leave,
    "agenda": cmd_agenda,
    "rooms": cmd_rooms,
    "free": cmd_free,
    "busy": cmd_busy,
    "who": cmd_who,
    "list": cmd_list,
}


def run(cal, words):
    cmd = words[0]
    if cmd not in COMMANDS:
        return ["error: unknown command " + cmd]
    return COMMANDS[cmd](cal, words[1:])
