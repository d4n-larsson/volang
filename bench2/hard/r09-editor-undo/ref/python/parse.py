"""Splitting command lines and resolving line addresses.

An address is a line number, `.` (the current line) or `$` (the last line).
A range is one address, or two separated by a comma: `2,5`, `.,$`.
"""
from buffer import EditError


def split_line(line):
    """The command word, the text after the first space, and whether there was a space."""
    i = line.find(" ")
    if i < 0:
        return line, "", False
    return line[:i], line[i + 1:], True


def split_first(rest):
    """(first word, text after the single space that follows it), or None without a space."""
    i = rest.find(" ")
    if i < 0:
        return None
    return rest[:i], rest[i + 1:]


def words(rest):
    return [w for w in rest.split(" ") if w != ""]


def is_number(s):
    return s != "" and all("0" <= c <= "9" for c in s)


def is_address(tok):
    return tok == "." or tok == "$" or is_number(tok)


def address_value(tok, buf):
    if tok == ".":
        return buf.cur
    if tok == "$":
        return buf.size()
    return int(tok)


def check_line(n, low, high):
    if n < low or n > high:
        raise EditError("line " + str(n) + " out of range")


def range_parts(tok):
    parts = tok.split(",")
    if len(parts) > 2 or not all(is_address(p) for p in parts):
        raise EditError("bad command")
    return parts


def resolve_range(tok, buf):
    """(a, b) for a range of existing lines."""
    parts = range_parts(tok)
    if buf.is_empty():
        raise EditError("empty buffer")
    a = address_value(parts[0], buf)
    b = address_value(parts[-1], buf)
    check_line(a, 1, buf.size())
    check_line(b, 1, buf.size())
    if a > b:
        raise EditError("bad range " + str(a) + "," + str(b))
    return a, b


def resolve_line(tok, buf):
    """One existing line."""
    if not is_address(tok):
        raise EditError("bad command")
    if buf.is_empty():
        raise EditError("empty buffer")
    n = address_value(tok, buf)
    check_line(n, 1, buf.size())
    return n


def resolve_insert(tok, buf):
    """A line to insert before: 1 to size + 1."""
    if not is_address(tok):
        raise EditError("bad command")
    n = address_value(tok, buf)
    check_line(n, 1, buf.size() + 1)
    return n


def resolve_after(tok, buf):
    """A line to put lines after: 0 (before the first line) to size."""
    if not is_address(tok):
        raise EditError("bad command")
    n = address_value(tok, buf)
    check_line(n, 0, buf.size())
    return n


def positive(tok):
    """A number of at least 1."""
    if not is_number(tok) or int(tok) < 1:
        raise EditError("bad command")
    return int(tok)
