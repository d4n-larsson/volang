"""A spreadsheet: reads commands from stdin, one per line, and prints what each one answers."""

import sys

import commands
from sheet import Sheet

HANDLERS = {
    "set": commands.cmd_set,
    "get": commands.cmd_get,
    "show": commands.cmd_show,
    "clear": commands.cmd_clear,
    "copy": commands.cmd_copy,
    "deps": commands.cmd_deps,
    "users": commands.cmd_users,
    "list": commands.cmd_list,
    "count": commands.cmd_count,
    "find": commands.cmd_find,
    "eval": commands.cmd_eval,
    "print": commands.cmd_print,
}


def run(sheet, words):
    handler = HANDLERS.get(words[0])
    if handler is None:
        return ["error: unknown command " + words[0]]
    try:
        return handler(sheet, words[1:])
    except commands.CommandError as e:
        return [str(e)]


def main():
    sheet = Sheet()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w]
        if words:
            for out in run(sheet, words):
                print(out)


main()
