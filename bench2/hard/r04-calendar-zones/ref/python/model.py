# Shared records. Every stored time is a number of minutes since day 0 00:00 UTC.

MINUTES_PER_DAY = 1440


class User:
    def __init__(self, name, offset):
        self.name = name
        # minutes to add to a UTC time to get the user's local time
        self.offset = offset
        # local working hours (first minute, minute after the last), or None
        self.hours = None


class Room:
    def __init__(self, name, capacity, offset):
        self.name = name
        self.capacity = capacity
        self.offset = offset


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
