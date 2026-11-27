"""How things are printed."""
import nodes
import paths

NO_ENTRY = "no such file or directory"
NOT_DIR = "not a directory"
IS_DIR = "is a directory"
EXISTS = "file exists"
NOT_EMPTY = "directory not empty"
IN_USE = "directory in use"
INTO_ITSELF = "cannot move a directory into itself"
DENIED = "permission denied"


def error(kind, names):
    """An error about the path NAMES, always shown in its absolute form."""
    return "error: " + kind + ": " + paths.display(names)


def is_no_entry(message):
    return message.startswith("error: " + NO_ENTRY + ": ")


def entry_name(name, node):
    """A name in a listing: directories end with `/`."""
    if node.is_dir:
        return name + "/"
    return name


def indent(depth):
    return "  " * depth


def stat_line(names, node):
    if node.is_dir:
        line = paths.display(names) + ": directory, " + str(nodes.entry_count(node)) + " entries"
    else:
        line = (paths.display(names) + ": file, " + str(nodes.file_size(node)) + " bytes, "
                + str(len(node.lines)) + " lines")
    return line + ", owner " + node.owner + ", mode " + node.mode


def long_line(name, node):
    """A line of `ls -l`: type and mode, owner, size (bytes, or entries of a directory), name."""
    if node.is_dir:
        kind, size = "d", nodes.entry_count(node)
    else:
        kind, size = "-", nodes.file_size(node)
    return kind + node.mode + " " + node.owner + " " + str(size) + " " + entry_name(name, node)


def size_line(size, names):
    return str(size) + " " + paths.display(names)


def numbered(number, line):
    return str(number) + ":" + line


def text_of(words):
    """The text of a `write` or `append`: the words joined by single spaces."""
    return " ".join(words)
