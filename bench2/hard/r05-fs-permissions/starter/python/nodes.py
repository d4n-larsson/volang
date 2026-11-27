"""The nodes of the tree: directories and files."""


class Node:
    """A directory (`is_dir`, its entries in `children` by name) or a file (its `lines` of text).

    A node does not know its own name: the name is the key under which its directory holds it."""

    def __init__(self, is_dir, lines):
        self.is_dir = is_dir
        self.children = {}
        self.lines = lines


def new_dir():
    return Node(True, [])


def new_file(lines):
    """A new file holding a copy of LINES."""
    return Node(False, list(lines))


def file_size(node):
    """Size of a file in bytes: every line counts with its newline."""
    total = 0
    for line in node.lines:
        total += len(line) + 1
    return total


def sorted_names(node):
    """The names of a directory's entries in ascending byte order."""
    names = list(node.children)
    names.sort()
    return names


def entry_count(node):
    return len(node.children)


def has_entry(node, name):
    return name in node.children


def entry(node, name):
    return node.children[name]


def put_entry(node, name, item):
    node.children[name] = item


def drop_entry(node, name):
    del node.children[name]
