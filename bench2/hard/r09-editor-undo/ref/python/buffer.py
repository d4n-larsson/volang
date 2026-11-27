"""The text buffer: a list of lines and a current-line pointer."""


class EditError(Exception):
    """A command that cannot run. Its message is printed after `error: `."""


class Buffer:
    def __init__(self):
        # Line N of the buffer is lines[N - 1].
        self.lines = []
        # The current line: 0 when the buffer is empty, else 1 to size().
        self.cur = 0

    def size(self):
        return len(self.lines)

    def is_empty(self):
        return len(self.lines) == 0

    def line(self, n):
        return self.lines[n - 1]

    def block(self, a, b):
        """A copy of lines a to b."""
        return self.lines[a - 1:b]

    def put_block(self, a, b, new_lines):
        """Replaces lines a to b (none when b == a - 1) with new_lines."""
        self.lines = self.lines[:a - 1] + list(new_lines) + self.lines[b:]

    def clamp_cur(self):
        """Keeps the current line inside the buffer after lines were removed."""
        if self.cur > self.size():
            self.cur = self.size()
