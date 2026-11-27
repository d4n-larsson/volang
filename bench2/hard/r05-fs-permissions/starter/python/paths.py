"""Paths. Every path argument is turned into the list of names that leads to it from the root
directory / before anything else happens: the walk to a node always starts at /."""


def normalize(cwd, text):
    """The names from / to the path TEXT, read relative to the current directory CWD (a list of names).

    Empty parts and `.` are skipped and `..` drops the last name (`..` of / is /). This is done on
    the text alone: `/a/b/..` is `/a` whatever `/a/b` is, and is never looked at."""
    names = [] if text.startswith("/") else list(cwd)
    for part in text.split("/"):
        if part == "" or part == ".":
            continue
        if part == "..":
            if names:
                names.pop()
            continue
        names.append(part)
    return names


def display(names):
    """The absolute form of a path: `/` for the root, else `/a/b`."""
    return "/" + "/".join(names)


def is_within(names, prefix):
    """Whether the path NAMES is the path PREFIX itself or lies below it."""
    if len(names) < len(prefix):
        return False
    for i in range(len(prefix)):
        if names[i] != prefix[i]:
            return False
    return True


def child(names, name):
    """The path of the entry NAME inside the directory NAMES."""
    return names + [name]


def parent(names):
    """The path of the directory holding NAMES (NAMES is not the root)."""
    return names[:-1]


def base_name(names):
    """The last name of a path that is not the root."""
    return names[-1]
