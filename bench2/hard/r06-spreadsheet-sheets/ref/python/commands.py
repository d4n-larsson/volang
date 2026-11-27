"""The commands. Each takes the book and the command's arguments and returns the lines to print."""

import cells
import evaluate
import grid
import parser
from formula import to_text, shift, referenced, bind


class CommandError(Exception):
    """A command that cannot be done: its message is printed instead and nothing changes."""


def need(args, n):
    if len(args) != n:
        raise CommandError("error: bad command")


def sheet_arg(book, name):
    s = book.by_name(name)
    if s is None:
        raise CommandError("error: unknown sheet " + name)
    return s


def qualified_arg(book, text, parse, kind):
    """(sheet id, what parse gives) for `NAME!TEXT` or, on the current sheet, `TEXT`."""
    name, rest = "", text
    if "!" in text:
        name, rest = text.split("!", 1)
    value = parse(rest)
    if value is None or (name != "" and not cells.is_sheet_name(name)) or (name == "" and "!" in text):
        raise CommandError("error: bad " + kind + " " + text)
    sid = book.current if name == "" else sheet_arg(book, name).id
    return sid, value


def cell_arg(book, text):
    """The place (sheet, col, row) of a cell argument."""
    sid, cell = qualified_arg(book, text, cells.parse_cell, "cell")
    return (sid,) + cell


def range_arg(book, text):
    sid, r = qualified_arg(book, text, cells.parse_range, "range")
    return (sid,) + r


def formula_arg(book, text, own):
    """The tree of `=FORMULA` for a formula on sheet `own`."""
    node = parser.parse_formula(text[1:]) if text.startswith("=") else None
    if node is None:
        raise CommandError("error: bad formula")
    unknown = bind(node, book, own)
    if unknown is not None:
        raise CommandError("error: unknown sheet " + unknown)
    return node


def place_name(book, place):
    """A place's name: qualified when it is not on the current sheet."""
    if place[0] == book.current:
        return cells.cell_name(place)
    return book.get(place[0]).name + "!" + cells.cell_name(place)


def filled_arg(book, place):
    c = book.cell(place)
    if c is None:
        raise CommandError("error: " + place_name(book, place) + " is empty")
    return c


def value_line(book, place):
    return place_name(book, place) + " = " + evaluate.value_text(evaluate.value_of(book, place))


def cmd_set(book, args):
    if len(args) < 2:
        raise CommandError("error: bad command")
    place = cell_arg(book, args[0])
    sheet = book.get(place[0])
    text = "".join(args[1:])
    if text.startswith("="):
        sheet.put_formula(place[1:], formula_arg(book, text, place[0]))
    elif cells.is_int(text):
        sheet.put_number(place[1:], int(text))
    else:
        raise CommandError("error: bad value")
    return [value_line(book, place)]


def cmd_get(book, args):
    need(args, 1)
    place = cell_arg(book, args[0])
    if book.cell(place) is None:
        return [place_name(book, place) + " empty"]
    return [value_line(book, place)]


def content_text(book, c):
    if c.node is None:
        return str(c.number)
    return "=" + to_text(c.node, book)


def cmd_show(book, args):
    need(args, 1)
    place = cell_arg(book, args[0])
    c = book.cell(place)
    return [place_name(book, place) + ": " + ("empty" if c is None else content_text(book, c))]


def cmd_clear(book, args):
    need(args, 1)
    place = cell_arg(book, args[0])
    filled_arg(book, place)
    book.get(place[0]).clear(place[1:])
    return [place_name(book, place) + " cleared"]


def cmd_copy(book, args):
    need(args, 2)
    src = cell_arg(book, args[0])
    dst = cell_arg(book, args[1])
    c = filled_arg(book, src)
    target = book.get(dst[0])
    if c.node is None:
        target.put_number(dst[1:], c.number)
    else:
        target.put_formula(dst[1:], shift(c.node, dst[1] - src[1], dst[2] - src[2], dst[0]))
    return ["copied " + place_name(book, src) + " to " + place_name(book, dst)]


def names_line(book, title, found):
    if not found:
        return title + ": none"
    return title + ": " + " ".join(place_name(book, p) for p in cells.sorted_cells(found))


def cmd_deps(book, args):
    need(args, 1)
    place = cell_arg(book, args[0])
    c = book.cell(place)
    found = [] if c is None or c.node is None else referenced(c.node, place[0], [])
    found = [p for p in found if book.get(p[0]) is not None]
    return [names_line(book, "deps " + place_name(book, place), found)]


def cmd_users(book, args):
    need(args, 1)
    place = cell_arg(book, args[0])
    found = []
    for sheet in book.in_order():
        for cell in sheet.formula_cells():
            if place in referenced(sheet.get(cell).node, sheet.id, []):
                found.append((sheet.id,) + cell)
    return [names_line(book, "users " + place_name(book, place), found)]


def cmd_list(book, args):
    need(args, 0)
    out = []
    sheet = book.get(book.current)
    for cell in sheet.filled():
        c = sheet.get(cell)
        line = cells.cell_name(cell) + ": " + content_text(book, c)
        if c.node is not None:
            line += " = " + evaluate.value_text(evaluate.value_of(book, (sheet.id,) + cell))
        out.append(line)
    return out if out else ["no cells"]


def cmd_count(book, args):
    need(args, 0)
    filled = formulas = errors = 0
    for sheet in book.in_order():
        filled += len(sheet.filled())
        formulas += len(sheet.formula_cells())
        errors += sum(1 for cell in sheet.filled() if evaluate.value_of(book, (sheet.id,) + cell)[0])
    return ["cells " + str(filled) + ", formulas " + str(formulas) + ", errors " + str(errors)]


def cmd_find(book, args):
    need(args, 1)
    if not cells.is_int(args[0]):
        raise CommandError("error: bad command")
    wanted = int(args[0])
    sheet = book.get(book.current)
    found = [(sheet.id,) + cell for cell in sheet.filled()
             if evaluate.value_of(book, (sheet.id,) + cell) == (evaluate.OK, wanted)]
    return [names_line(book, "found", found)]


def cmd_eval(book, args):
    if len(args) < 1:
        raise CommandError("error: bad command")
    node = formula_arg(book, "".join(args), book.current)
    return ["= " + evaluate.value_text(evaluate.formula_value(book, node, book.current))]


def cmd_print(book, args):
    need(args, 1)
    r = range_arg(book, args[0])
    return grid.render(book, *r)


def cmd_sheet(book, args):
    need(args, 1)
    name = args[0]
    if not cells.is_sheet_name(name):
        raise CommandError("error: bad sheet name " + name)
    if book.by_name(name) is not None:
        raise CommandError("error: sheet " + name + " exists")
    book.add(name)
    return ["sheet " + name + " added"]


def cmd_use(book, args):
    need(args, 1)
    book.current = sheet_arg(book, args[0]).id
    return ["using " + args[0]]


def cmd_sheets(book, args):
    need(args, 0)
    out = []
    for sheet in book.in_order():
        line = sheet.name + ": " + str(len(sheet.cells)) + " cells"
        if sheet.id == book.current:
            line += " (current)"
        out.append(line)
    return out


def cmd_rename(book, args):
    need(args, 2)
    old, new = args
    sheet = sheet_arg(book, old)
    if not cells.is_sheet_name(new):
        raise CommandError("error: bad sheet name " + new)
    if book.by_name(new) is not None:
        raise CommandError("error: sheet " + new + " exists")
    sheet.name = new
    return ["renamed " + old + " to " + new]


def cmd_drop(book, args):
    need(args, 1)
    sheet = sheet_arg(book, args[0])
    if sheet.id == book.current:
        raise CommandError("error: cannot drop the current sheet")
    book.drop(sheet.id)
    return ["dropped " + args[0]]
