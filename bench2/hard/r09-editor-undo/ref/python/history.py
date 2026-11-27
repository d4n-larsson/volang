"""Undo history of one buffer."""


class Step:
    """One undo step: the lines and current line just before and just after it."""

    def __init__(self, before_lines, before_cur, after_lines, after_cur):
        self.before_lines = before_lines
        self.before_cur = before_cur
        self.after_lines = after_lines
        self.after_cur = after_cur


class History:
    def __init__(self):
        self.undo = []
        self.redo = []
        # The position (number of undo steps) at the last save; -1 when it can no longer be reached.
        self.saved = 0

    def position(self):
        return len(self.undo)

    def modified(self):
        return self.saved != self.position()

    def add_step(self, step):
        if self.saved > self.position():
            self.saved = -1
        self.undo.append(step)
        self.redo = []

    def merge_since(self, start):
        """Turns the steps recorded since position `start` into one; returns how many there were."""
        steps = self.undo[start:]
        if len(steps) > 1:
            self.undo[start:] = [Step(steps[0].before_lines, steps[0].before_cur,
                                      steps[-1].after_lines, steps[-1].after_cur)]
        return len(steps)
