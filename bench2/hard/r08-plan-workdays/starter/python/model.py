# Shared records of the planner.


class Dep:
    """A finish-to-start dependency: the task starts LAG days after PRED finishes."""

    def __init__(self, pred, lag):
        self.pred = pred
        self.lag = lag


class Task:
    def __init__(self, tid, name, duration, seq):
        self.tid = tid
        self.name = name
        self.duration = duration      # days; 0 is a milestone
        self.seq = seq                # creation order
        self.preds = []               # list of Dep, in the order they were added
        self.not_before = 0           # earliest allowed start day
        self.done = 0                 # percent complete


class Slot:
    """Where a task sits in the schedule. `finish` is the first day after its work."""

    def __init__(self, start, finish):
        self.start = start
        self.finish = finish
        self.lstart = 0
        self.lfinish = 0
        self.slack = 0


class Plan:
    def __init__(self):
        self.tasks = {}
        self.next_seq = 1
