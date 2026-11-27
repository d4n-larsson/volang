"""Values of cells and formulas. A value is (error, number): error is "" or #DIV0, #CYCLE or #REF.

A formula's value is the first error met when evaluating it left to right (a function's range row by
row, left to right), else its number. An empty cell counts as 0; SUM adds the range's cells, MIN and
MAX skip its empty cells (0 when all are empty). Division truncates toward zero.
"""

import cells
from formula import Num, CellRef, Func, Bin, Neg, Paren, range_of

OK = ""


def value_text(value):
    err, n = value
    return err if err else str(n)


def divide(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def cell_value(sheet, cell, active):
    """The value of a cell. `active` holds the cells being evaluated: meeting one again is a cycle."""
    c = sheet.get(cell)
    if c is None:
        return (OK, 0)
    if c.node is None:
        return (OK, c.number)
    if cell in active:
        return ("#CYCLE", 0)
    active.add(cell)
    v = node_value(sheet, c.node, active)
    active.discard(cell)
    return v


def func_value(sheet, func, active):
    if func.first is None:
        return ("#REF", 0)
    total = 0
    best = None
    for cell in cells.range_cells(*range_of(func)):
        if sheet.get(cell) is None:
            continue
        err, n = cell_value(sheet, cell, active)
        if err:
            return (err, 0)
        total += n
        if best is None or (func.name == "MIN" and n < best) or (func.name == "MAX" and n > best):
            best = n
    if func.name == "SUM":
        return (OK, total)
    return (OK, 0 if best is None else best)


def node_value(sheet, node, active):
    if isinstance(node, Num):
        return (OK, node.value)
    if isinstance(node, CellRef):
        return cell_value(sheet, (node.ref.col, node.ref.row), active)
    if isinstance(node, Func):
        return func_value(sheet, node, active)
    if isinstance(node, Bin):
        left = node_value(sheet, node.left, active)
        if left[0]:
            return left
        right = node_value(sheet, node.right, active)
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
        return node_value(sheet, node.inner, active)
    if isinstance(node, Neg):
        inner = node_value(sheet, node.inner, active)
        if inner[0]:
            return inner
        return (OK, -inner[1])
    return ("#REF", 0)


def value_of(sheet, cell):
    return cell_value(sheet, cell, set())


def formula_value(sheet, node):
    return node_value(sheet, node, set())
