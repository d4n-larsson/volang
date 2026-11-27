"""The commands. Each takes the sheet and the command's arguments and returns the lines to print."""

import cells
import evaluate
import grid
import parser
from formula import to_text, shift, referenced


class CommandError(Exception):
    """A command that cannot be done: its message is printed instead and nothing changes."""


def need(args, n):
    if len(args) != n:
        raise CommandError("error: bad command")


def cell_arg(text):
    cell = cells.parse_cell(text)
    if cell is None:
        raise CommandError("error: bad cell " + text)
    return cell


def range_arg(text):
    r = cells.parse_range(text)
    if r is None:
        raise CommandError("error: bad range " + text)
    return r


def formula_arg(text):
    """The tree of `=FORMULA`."""
    node = parser.parse_formula(text[1:]) if text.startswith("=") else None
    if node is None:
        raise CommandError("error: bad formula")
    return node


def filled_arg(sheet, cell):
    c = sheet.get(cell)
    if c is None:
        raise CommandError("error: " + cells.cell_name(cell) + " is empty")
    return c


def value_line(sheet, cell):
    return cells.cell_name(cell) + " = " + evaluate.value_text(evaluate.value_of(sheet, cell))


def cmd_set(sheet, args):
    if len(args) < 2:
        raise CommandError("error: bad command")
    cell = cell_arg(args[0])
    text = "".join(args[1:])
    if text.startswith("="):
        sheet.put_formula(cell, formula_arg(text))
    elif cells.is_int(text):
        sheet.put_number(cell, int(text))
    else:
        raise CommandError("error: bad value")
    return [value_line(sheet, cell)]


def cmd_get(sheet, args):
    need(args, 1)
    cell = cell_arg(args[0])
    if sheet.get(cell) is None:
        return [cells.cell_name(cell) + " empty"]
    return [value_line(sheet, cell)]


def content_text(c):
    if c.node is None:
        return str(c.number)
    return "=" + to_text(c.node)


def cmd_show(sheet, args):
    need(args, 1)
    cell = cell_arg(args[0])
    c = sheet.get(cell)
    return [cells.cell_name(cell) + ": " + ("empty" if c is None else content_text(c))]


def cmd_clear(sheet, args):
    need(args, 1)
    cell = cell_arg(args[0])
    filled_arg(sheet, cell)
    sheet.clear(cell)
    return [cells.cell_name(cell) + " cleared"]


def cmd_copy(sheet, args):
    need(args, 2)
    src = cell_arg(args[0])
    dst = cell_arg(args[1])
    c = filled_arg(sheet, src)
    if c.node is None:
        sheet.put_number(dst, c.number)
    else:
        sheet.put_formula(dst, shift(c.node, dst[0] - src[0], dst[1] - src[1]))
    return ["copied " + cells.cell_name(src) + " to " + cells.cell_name(dst)]


def names_line(title, found):
    if not found:
        return title + ": none"
    return title + ": " + " ".join(cells.cell_name(c) for c in cells.sorted_cells(found))


def cmd_deps(sheet, args):
    need(args, 1)
    cell = cell_arg(args[0])
    c = sheet.get(cell)
    found = [] if c is None or c.node is None else referenced(c.node, [])
    return [names_line("deps " + cells.cell_name(cell), found)]


def cmd_users(sheet, args):
    need(args, 1)
    cell = cell_arg(args[0])
    found = [f for f in sheet.formula_cells() if cell in referenced(sheet.get(f).node, [])]
    return [names_line("users " + cells.cell_name(cell), found)]


def cmd_list(sheet, args):
    need(args, 0)
    out = []
    for cell in sheet.filled():
        c = sheet.get(cell)
        line = cells.cell_name(cell) + ": " + content_text(c)
        if c.node is not None:
            line += " = " + evaluate.value_text(evaluate.value_of(sheet, cell))
        out.append(line)
    return out if out else ["no cells"]


def cmd_count(sheet, args):
    need(args, 0)
    filled = sheet.filled()
    formulas = len(sheet.formula_cells())
    errors = sum(1 for cell in filled if evaluate.value_of(sheet, cell)[0])
    return ["cells " + str(len(filled)) + ", formulas " + str(formulas) + ", errors " + str(errors)]


def cmd_find(sheet, args):
    need(args, 1)
    if not cells.is_int(args[0]):
        raise CommandError("error: bad command")
    wanted = int(args[0])
    found = [cell for cell in sheet.filled() if evaluate.value_of(sheet, cell) == (evaluate.OK, wanted)]
    return [names_line("found", found)]


def cmd_eval(sheet, args):
    if len(args) < 1:
        raise CommandError("error: bad command")
    node = formula_arg("".join(args))
    return ["= " + evaluate.value_text(evaluate.formula_value(sheet, node))]


def cmd_print(sheet, args):
    need(args, 1)
    return grid.render(sheet, *range_arg(args[0]))
