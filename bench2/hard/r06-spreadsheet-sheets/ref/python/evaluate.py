"""Values of cells and formulas. A value is (error, number): error is "" or #DIV0, #CYCLE or #REF.

A formula's value is the first error met when evaluating it left to right (a function's range row by
row, left to right), else its number. An empty cell counts as 0; SUM adds the range's cells, MIN and
MAX skip its empty cells (0 when all are empty). Division truncates toward zero. A reference to a
sheet that was dropped is #REF.
"""

import cells
from formula import Num, CellRef, Func, Bin, Neg, Paren, range_of, ref_sheet

OK = ""


def value_text(value):
    err, n = value
    return err if err else str(n)


def divide(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def cell_value(book, place, active):
    """The value of a place (sheet, col, row). `active` holds the places being evaluated: meeting
    one again is a cycle."""
    c = book.cell(place)
    if c is None:
        return (OK, 0)
    if c.node is None:
        return (OK, c.number)
    if place in active:
        return ("#CYCLE", 0)
    active.add(place)
    v = node_value(book, c.node, place[0], active)
    active.discard(place)
    return v


def func_value(book, func, own, active):
    if func.first is None:
        return ("#REF", 0)
    sid = ref_sheet(func.first, own)
    if book.get(sid) is None:
        return ("#REF", 0)
    total = 0
    best = None
    for col, row in cells.range_cells(*range_of(func)):
        place = (sid, col, row)
        if book.cell(place) is None:
            continue
        err, n = cell_value(book, place, active)
        if err:
            return (err, 0)
        total += n
        if best is None or (func.name == "MIN" and n < best) or (func.name == "MAX" and n > best):
            best = n
    if func.name == "SUM":
        return (OK, total)
    return (OK, 0 if best is None else best)


def node_value(book, node, own, active):
    """The value of a tree of a formula on sheet `own`."""
    if isinstance(node, Num):
        return (OK, node.value)
    if isinstance(node, CellRef):
        sid = ref_sheet(node.ref, own)
        if book.get(sid) is None:
            return ("#REF", 0)
        return cell_value(book, (sid, node.ref.col, node.ref.row), active)
    if isinstance(node, Func):
        return func_value(book, node, own, active)
    if isinstance(node, Bin):
        left = node_value(book, node.left, own, active)
        if left[0]:
            return left
        right = node_value(book, node.right, own, active)
        if right[0]:
            return right
        a, b = left[1], right[1]
        if node.op == "+":
            return (OK, a + b)
        if node.op == "-":
            return (OK, a - b)
        if node.op == "*":
            return (OK, a * b)
        if b == 0:
            return ("#DIV0", 0)
        return (OK, divide(a, b))
    if isinstance(node, Paren):
        return node_value(book, node.inner, own, active)
    if isinstance(node, Neg):
        inner = node_value(book, node.inner, own, active)
        if inner[0]:
            return inner
        return (OK, -inner[1])
    return ("#REF", 0)


def value_of(book, place):
    return cell_value(book, place, set())


def formula_value(book, node, own):
    return node_value(book, node, own, set())
