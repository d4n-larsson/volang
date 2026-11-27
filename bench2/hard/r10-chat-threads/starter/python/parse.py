"""Splitting command lines and checking their arguments.

Every check that fails raises Failure; the dispatcher prints `error: ` followed by its message.
"""

NAME_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789"
MAX_NAME = 16
MAX_NUMBER_DIGITS = 6


class Failure(Exception):
    """A command that cannot be carried out. str(failure) is the text after `error: `."""


class BadCommand(Failure):
    def __init__(self):
        super().__init__("bad command")


def split_words(line):
    """The words of a line: runs of characters other than a space."""
    return [w for w in line.split(" ") if w != ""]


def is_name(s):
    return 1 <= len(s) <= MAX_NAME and all(c in NAME_CHARS for c in s)


def is_number(s):
    return 1 <= len(s) <= MAX_NUMBER_DIGITS and all("0" <= c <= "9" for c in s)


def expect_count(args, least, most):
    """The command takes from `least` to `most` arguments (most -1: no upper limit)."""
    if len(args) < least or (most >= 0 and len(args) > most):
        raise BadCommand()


def name_arg(s):
    if not is_name(s):
        raise BadCommand()
    return s


def number_arg(s, least):
    if not is_number(s) or int(s) < least:
        raise BadCommand()
    return int(s)


def text_arg(words):
    """The message text: the remaining words joined by single spaces."""
    if len(words) == 0:
        raise BadCommand()
    return " ".join(words)


def flag_arg(s, flag):
    """An optional keyword such as `private`: anything else is a bad command."""
    if s != flag:
        raise BadCommand()
    return True
