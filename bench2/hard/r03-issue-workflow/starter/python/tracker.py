"""The tracker's records: users, issues, comments, the event log and the clock."""


class Comment:
    def __init__(self, tick, author, text):
        self.tick = tick
        self.author = author
        self.text = text


class Issue:
    def __init__(self, num, title, priority, tick):
        self.num = num
        self.title = title
        self.priority = priority      # 1 (most urgent) to 4
        self.assignee = ""            # "" when nobody is assigned
        self.labels = []              # kept sorted
        self.comments = []
        self.status = "open"          # "open" or "closed"
        self.created = tick
        self.updated = tick


class Event:
    def __init__(self, tick, message):
        self.tick = tick
        self.message = message


class Tracker:
    def __init__(self):
        self.users = {}       # name -> tick the user was added
        self.issues = {}      # number -> Issue, in creation order
        self.events = []      # every successful change, oldest first
        self.next_num = 1
        self.clock = 0        # the tick of the command being run


def is_open(issue):
    return issue.status == "open"


def touch(t, issue):
    issue.updated = t.clock


def log_change(t, message):
    """Logs a successful change and returns its message as the command's output."""
    t.events.append(Event(t.clock, message))
    return message
