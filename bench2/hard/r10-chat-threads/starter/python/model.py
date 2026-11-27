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


class Message:
    def __init__(self, msg_id, channel, author, text, time):
        self.msg_id = msg_id
        self.channel = channel      # the channel's current name
        self.author = author
        self.text = text
        self.time = time            # the clock value of the command that posted it
        self.edited = False


class State:
    def __init__(self):
        self.users = {}
        self.channels = {}
        # every message that has not been deleted, in the order posted (ascending msg_id)
        self.messages = []
        self.next_id = 1
        # the logical clock: the number of the current command (non-blank input lines from 1)
        self.time = 0
        self.edits = 0
        self.deletes = 0
