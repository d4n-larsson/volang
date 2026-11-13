class Task:
    def __init__(self, id, prio, text):
        self.id, self.prio, self.text = id, prio, text
        self.done = False
        self.done_seq = 0
        self.tags = []


class Store:
    def __init__(self):
        self.tasks = {}  # decimal id string -> Task
        self.next_id = 1
        self.seq = 0

    def add(self, prio, text):
        t = Task(self.next_id, prio, text)
        self.tasks[str(t.id)] = t
        self.next_id += 1
        return t.id

    def get(self, field):
        return self.tasks.get(field)

    def set_done(self, t, done):
        """False if the task already had that state."""
        if t.done == done:
            return False
        t.done = done
        if done:
            self.seq += 1
            t.done_seq = self.seq
        return True

    def delete(self, t):
        del self.tasks[str(t.id)]

    def add_tag(self, t, tag):
        if tag in t.tags:
            return False
        t.tags.append(tag)
        return True

    def all(self):
        return sorted(self.tasks.values(), key=lambda t: t.id)
