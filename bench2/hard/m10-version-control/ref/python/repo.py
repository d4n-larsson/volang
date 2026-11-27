"""The repository: commits, branches, revisions, ancestry and tree changes."""


class Commit:
    def __init__(self, number, parents, message, snapshot):
        self.number = number
        self.parents = parents
        self.message = message
        self.snapshot = snapshot


class RepoError(Exception):
    pass


def is_number(s):
    return s != "" and all("0" <= c <= "9" for c in s)


def byte_sorted(paths):
    return sorted(paths, key=lambda p: p.encode())


def changes(x, y):
    out = []
    for p in byte_sorted(set(x) | set(y)):
        if p not in x:
            out.append("A " + p)
        elif p not in y:
            out.append("D " + p)
        elif x[p] != y[p]:
            out.append("M " + p)
    return out


def summary(x, y):
    ch = changes(x, y)
    count = lambda k: sum(1 for c in ch if c[0] == k)
    return f"({count('A')} added, {count('M')} modified, {count('D')} deleted)"


class Repo:
    def __init__(self):
        self.commits = {}
        self.branches = {"main": None}
        self.current = "main"
        self.work = {}

    def head(self):
        return self.branches[self.current]

    def head_snapshot(self):
        h = self.head()
        return {} if h is None else self.commits[h].snapshot

    def dirty(self):
        return bool(changes(self.head_snapshot(), self.work))

    def resolve(self, rev):
        n = self.lookup(rev)
        if n is None:
            raise RepoError(f"unknown revision {rev}")
        return n

    def lookup(self, rev):
        """The commit number rev names, or None."""
        if "~" in rev:
            x, steps = rev.rsplit("~", 1)
            if not is_number(steps):
                return None
            n = self.lookup(x)
            for _ in range(int(steps)):
                if n is None or not self.commits[n].parents:
                    return None
                n = self.commits[n].parents[0]
            return n
        if is_number(rev):
            return int(rev) if int(rev) in self.commits else None
        return self.branches.get(rev)

    def contained(self, a, b):
        """All commits contained in b, as a set; test membership of a."""
        return a in self.ancestry(b)

    def ancestry(self, b):
        seen, todo = set(), [b]
        while todo:
            c = todo.pop()
            if c not in seen:
                seen.add(c)
                todo += self.commits[c].parents
        return seen

    def make_commit(self, parents, message, snapshot):
        n = len(self.commits) + 1
        self.commits[n] = Commit(n, parents, message, dict(snapshot))
        return n
