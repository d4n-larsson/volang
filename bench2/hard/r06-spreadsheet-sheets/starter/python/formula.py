"""Formula trees: what the parser builds, and what is shown, shifted and evaluated."""

import cells


class Ref:
    """A cell reference; abs_col / abs_row are the `$` marks that keep a part fixed when copied."""

    def __init__(self, col, row, abs_col, abs_row):
        self.col = col
        self.row = row
        self.abs_col = abs_col
        self.abs_row = abs_row


class Num:
    def __init__(self, value):
        self.value = value


class CellRef:
    def __init__(self, ref):
        self.ref = ref


class Func:
    """SUM, MIN or MAX over the range first:last; `first` is None when the range was lost (#REF)."""

    def __init__(self, name, first, last):
        self.name = name
        self.first = first
        self.last = last


class Bin:
    def __init__(self, op, left, right):
        self.op = op
        self.left = left
        self.right = right


class Neg:
    def __init__(self, inner):
        self.inner = inner


class Paren:
    def __init__(self, inner):
        self.inner = inner


class BadRef:
    """A reference that was copied off the grid."""


def ref_text(ref):
    return ("$" if ref.abs_col else "") + cells.COLS[ref.col] + ("$" if ref.abs_row else "") + str(ref.row)


def to_text(node):
    """The formula as `show` prints it (without the leading `=`)."""
    if isinstance(node, Num):
        return str(node.value)
    if isinstance(node, CellRef):
        return ref_text(node.ref)
    if isinstance(node, Func):
        if node.first is None:
            return node.name + "(#REF)"
        return node.name + "(" + ref_text(node.first) + ":" + ref_text(node.last) + ")"
    if isinstance(node, Bin):
        return to_text(node.left) + node.op + to_text(node.right)
    if isinstance(node, Neg):
        return "-" + to_text(node.inner)
    if isinstance(node, Paren):
        return "(" + to_text(node.inner) + ")"
    return "#REF"


def shift_ref(ref, dcol, drow):
    """The reference moved by dcol columns and drow rows (except its `$` parts), or None off the grid."""
    col = ref.col if ref.abs_col else ref.col + dcol
    row = ref.row if ref.abs_row else ref.row + drow
    if not cells.on_grid(col, row):
        return None
    return Ref(col, row, ref.abs_col, ref.abs_row)


def shift(node, dcol, drow):
    """A copy of the tree for a formula copied dcol columns right and drow rows down."""
    if isinstance(node, CellRef):
        moved = shift_ref(node.ref, dcol, drow)
        if moved is None:
            return BadRef()
        return CellRef(moved)
    if isinstance(node, Func):
        if node.first is None:
            return node
        first = shift_ref(node.first, dcol, drow)
        last = shift_ref(node.last, dcol, drow)
        if first is None or last is None:
            return Func(node.name, None, None)
        return Func(node.name, first, last)
    if isinstance(node, Bin):
        return Bin(node.op, shift(node.left, dcol, drow), shift(node.right, dcol, drow))
    if isinstance(node, Neg):
        return Neg(shift(node.inner, dcol, drow))
    if isinstance(node, Paren):
        return Paren(shift(node.inner, dcol, drow))
    return node


def range_of(func):
    """(col1, row1, col2, row2) of a function's range, corners in order."""
    a, b = func.first, func.last
    return (min(a.col, b.col), min(a.row, b.row), max(a.col, b.col), max(a.row, b.row))


def referenced(node, out):
    """Adds to `out` every cell the tree refers to, ranges cell by cell."""
    if isinstance(node, CellRef):
        out.append((node.ref.col, node.ref.row))
    elif isinstance(node, Func):
        if node.first is not None:
            out.extend(cells.range_cells(*range_of(node)))
    elif isinstance(node, Bin):
        referenced(node.left, out)
        referenced(node.right, out)
    elif isinstance(node, (Neg, Paren)):
        referenced(node.inner, out)
    return out
