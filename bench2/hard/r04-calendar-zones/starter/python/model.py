# Shared records. Every time is a number of minutes since day 0 00:00.

MINUTES_PER_DAY = 1440


class User:
    def __init__(self, name):
        self.name = name


class Room:
    def __init__(self, name, capacity):
        self.name = name
        self.capacity = capacity


class Meeting:
    def __init__(self, meeting_id, room, organizer, start, end, attendees):
        self.meeting_id = meeting_id
        self.room = room
        self.organizer = organizer
        # start is the first minute of the meeting, end the minute right after it
        self.start = start
        self.end = end
        self.attendees = attendees

    def people(self):
        return [self.organizer] + self.attendees

    def length(self):
        return self.end - self.start


class Calendar:
    def __init__(self):
        self.users = {}
        self.rooms = {}
        self.meetings = {}
