"""The tracker's records: users, issues, comments, the event log and the clock."""


class Comment:
    def __init__(self, tick, author, text):
        self.tick = tick
        self.author = author
        self.text = text


class Transition:
    def __init__(self, tick, source, target, reason):
        self.tick = tick
        self.source = source
        self.target = target
        self.reason = reason          # "" when none was given


STATES = ["new", "triaged", "in_progress", "blocked", "review", "done", "wontfix"]
TERMINAL = ["done", "wontfix"]
ALLOWED = {
    "new": ["triaged", "wontfix"],
    "triaged": ["in_progress", "wontfix"],
    "in_progress": ["triaged", "blocked", "review"],
    "blocked": ["triaged", "in_progress"],
    "review": ["in_progress", "done"],
    "done": ["triaged"],
    "wontfix": ["triaged"],
}


class Issue:
    def __init__(self, num, title, priority, tick):
        self.num = num
        self.title = title
        self.priority = priority      # 1 (most urgent) to 4
        self.assignee = ""            # "" when nobody is assigned
        self.labels = []              # kept sorted
        self.comments = []
        self.status = "new"           # one of STATES
        self.history = []             # Transition, oldest first
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
    return issue.status not in TERMINAL


def transition(t, issue, target, reason):
    issue.history.append(Transition(t.clock, issue.status, target, reason))
    issue.status = target
    touch(t, issue)


def touch(t, issue):
    issue.updated = t.clock


def log_change(t, message):
    """Logs a successful change and returns its message as the command's output."""
    t.events.append(Event(t.clock, message))
    return message
