"""The records the chat server keeps."""


class User:
    def __init__(self, name):
        self.name = name


class Channel:
    def __init__(self, name, creator, private):
        self.name = name
        self.creator = creator
        self.private = private
        # user names in the order they joined (a user who leaves and joins again goes last)
        self.members = []
        # user names that may join a private channel; the creator is always invited
        self.invited = set()
        # the channel's messages by number, ascending: live ones and deleted thread parents
        self.messages = {}
        self.next_number = 1
        # each member's read marker: the clock value up to which they have read the channel
        self.markers = {}


class Message:
    def __init__(self, number, author, text, time, parent):
        self.msg_id = number        # the number within its channel
        self.author = author
        self.text = text
        self.time = time            # the clock value of the command that posted it
        self.edited = False
        self.parent = parent        # the thread parent's number, 0 for a top-level message
        self.deleted = False        # a deleted thread parent kept for its replies


class State:
    def __init__(self):
        self.users = {}
        self.channels = {}
        # the logical clock: the number of the current command (non-blank input lines from 1)
        self.time = 0
        self.edits = 0
        self.deletes = 0


def live_messages(channel):
    """The channel's messages that are not deleted, in number order."""
    return [m for m in channel.messages.values() if not m.deleted]


def replies(channel, number):
    return [m for m in channel.messages.values() if m.parent == number]


def all_live(state):
    """(channel, message) for every live message, in the order posted."""
    found = [(c, m) for c in state.channels.values() for m in live_messages(c)]
    return sorted(found, key=lambda cm: cm[1].time)
