"""The tree: entries, path resolution, sizes and absolute paths."""


class Entry:
    def __init__(self, name, parent, is_dir):
        self.name = name
        self.parent = parent
        self.children = {} if is_dir else None
        self.text = ""

    @property
    def is_dir(self):
        return self.children is not None


class FsError(Exception):
    pass


class Tree:
    def __init__(self):
        self.root = Entry("", None, True)
        self.root.parent = self.root
        self.cwd = self.root
        self.limit = None

    def components(self, path):
        return [c for c in path.split("/") if c != ""]

    def start(self, path):
        return self.root if path.startswith("/") else self.cwd

    def step(self, cur, comp):
        if not cur.is_dir:
            raise FsError("not a directory")
        if comp == ".":
            return cur
        if comp == "..":
            return cur.parent
        if comp not in cur.children:
            raise FsError("no such file or directory")
        return cur.children[comp]

    def resolve(self, path):
        cur = self.start(path)
        for c in self.components(path):
            cur = self.step(cur, c)
        return cur

    def locate(self, path):
        """(directory, name, existing entry or None); name is None for a whole-path directory."""
        comps = self.components(path)
        if not comps or comps[-1] in (".", ".."):
            return None, None, self.resolve(path)
        cur = self.start(path)
        for c in comps[:-1]:
            cur = self.step(cur, c)
        if not cur.is_dir:
            raise FsError("not a directory")
        return cur, comps[-1], cur.children.get(comps[-1])

    def size(self, e):
        if not e.is_dir:
            return len(e.text)
        return sum(self.size(c) for c in e.children.values())

    def usage(self):
        return self.size(self.root)

    def abspath(self, e):
        parts = []
        while e is not self.root:
            parts.append(e.name)
            e = e.parent
        return "/" + "/".join(reversed(parts))

    def is_within(self, e, top):
        """e is top or lies anywhere below it."""
        while True:
            if e is top:
                return True
            if e is self.root:
                return False
            e = e.parent

    def add(self, directory, name, e):
        e.name = name
        e.parent = directory
        directory.children[name] = e

    def detach(self, e):
        del e.parent.children[e.name]

    def copy(self, e):
        c = Entry(e.name, None, e.is_dir)
        c.text = e.text
        if e.is_dir:
            for n, ch in e.children.items():
                cc = self.copy(ch)
                cc.parent = c
                c.children[n] = cc
        return c

    def fits(self, new_usage):
        return self.limit is None or new_usage <= self.limit
