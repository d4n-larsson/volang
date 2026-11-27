"""The command dispatcher: checks each command's arguments and runs it."""
import edits
import search
import transform
import view
from buffer import EditError
from parse import (positive, resolve_after, resolve_insert, resolve_line, resolve_range,
                   split_first, split_line, words, is_number)


def run(line, buf):
    """The output lines of one command."""
    cmd, rest, has_rest = split_line(line)
    try:
        if cmd in TEXT_COMMANDS:
            return TEXT_COMMANDS[cmd](buf, rest, has_rest)
        if cmd in WORD_COMMANDS:
            return WORD_COMMANDS[cmd](buf, words(rest))
        raise EditError("unknown command " + cmd)
    except EditError as e:
        return ["error: " + str(e)]


def need(args, n):
    if len(args) != n:
        raise EditError("bad command")


# ---- commands whose last argument is free text (kept exactly, spaces included) ----

def cmd_append(buf, rest, has_rest):
    if not has_rest:
        raise EditError("bad command")
    return edits.append(buf, rest)


def cmd_insert(buf, rest, has_rest):
    parts = split_first(rest)
    if parts is None:
        raise EditError("bad command")
    return edits.insert(buf, resolve_insert(parts[0], buf), parts[1])


def cmd_replace(buf, rest, has_rest):
    parts = split_first(rest)
    if parts is None:
        raise EditError("bad command")
    return edits.replace(buf, resolve_line(parts[0], buf), parts[1])


def cmd_find(buf, rest, has_rest):
    if rest == "":
        raise EditError("bad command")
    return search.find_text(buf, rest)


TEXT_COMMANDS = {
    "append": cmd_append,
    "insert": cmd_insert,
    "replace": cmd_replace,
    "find": cmd_find,
}


# ---- commands made of words ----

def range_command(f):
    """A command taking exactly one range."""
    def run_range(buf, args):
        need(args, 1)
        a, b = resolve_range(args[0], buf)
        return f(buf, a, b)
    return run_range


def cmd_print(buf, args):
    if len(args) == 0:
        if buf.is_empty():
            raise EditError("empty buffer")
        return view.print_range(buf, 1, buf.size())
    need(args, 1)
    a, b = resolve_range(args[0], buf)
    return view.print_range(buf, a, b)


def cmd_goto(buf, args):
    need(args, 1)
    return view.goto(buf, resolve_line(args[0], buf))


def cmd_move(buf, args):
    need(args, 2)
    a, b = resolve_range(args[0], buf)
    return edits.move(buf, a, b, resolve_after(args[1], buf))


def cmd_join(buf, args):
    need(args, 1)
    a, b = resolve_range(args[0], buf)
    if "," not in args[0]:
        # One address joins that line with the next one.
        if a == buf.size():
            raise EditError("nothing to join")
        b = a + 1
    return edits.join_lines(buf, a, b)


def cmd_split(buf, args):
    need(args, 2)
    at = resolve_line(args[0], buf)
    if not is_number(args[1]):
        raise EditError("bad command")
    return edits.split_at(buf, at, int(args[1]))


def cmd_subst(buf, args):
    need(args, 3)
    a, b = resolve_range(args[0], buf)
    return transform.subst(buf, a, b, args[1], args[2])


def cmd_indent(buf, args):
    need(args, 2)
    a, b = resolve_range(args[0], buf)
    n = positive(args[1])
    if n > 8:
        raise EditError("bad command")
    return transform.indent(buf, a, b, n)


def cmd_wrap(buf, args):
    need(args, 2)
    a, b = resolve_range(args[0], buf)
    return transform.wrap(buf, a, b, positive(args[1]))


def cmd_dedupe(buf, args):
    need(args, 0)
    return transform.dedupe(buf)


def cmd_clear(buf, args):
    need(args, 0)
    return edits.clear(buf)


def cmd_count(buf, args):
    need(args, 0)
    return search.count(buf)


def cmd_where(buf, args):
    need(args, 0)
    return view.where(buf)


def cmd_longest(buf, args):
    need(args, 0)
    if buf.is_empty():
        raise EditError("empty buffer")
    return view.longest(buf)


WORD_COMMANDS = {
    "delete": range_command(edits.delete),
    "dup": range_command(edits.dup),
    "upper": range_command(transform.upper_lines),
    "lower": range_command(transform.lower_lines),
    "strip": range_command(transform.strip),
    "sort": range_command(transform.sort_lines),
    "reverse": range_command(transform.reverse),
    "print": cmd_print,
    "goto": cmd_goto,
    "move": cmd_move,
    "join": cmd_join,
    "split": cmd_split,
    "subst": cmd_subst,
    "indent": cmd_indent,
    "wrap": cmd_wrap,
    "dedupe": cmd_dedupe,
    "clear": cmd_clear,
    "count": cmd_count,
    "where": cmd_where,
    "longest": cmd_longest,
}
