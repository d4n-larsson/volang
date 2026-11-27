"""Formula trees: what the parser builds, and what is shown, shifted and evaluated."""

import cells


OWN = -1


class Ref:
    """A cell reference; abs_col / abs_row are the `$` marks that keep a part fixed when copied.
    sheet is the id of the sheet it names, or OWN for the formula's own sheet; qualifier is the sheet
    name written in the formula, until bind() turns it into the id."""

    def __init__(self, col, row, abs_col, abs_row, sheet=OWN, qualifier=""):
        self.col = col
        self.row = row
        self.abs_col = abs_col
        self.abs_row = abs_row
        self.sheet = sheet
        self.qualifier = qualifier


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


def prefix(ref, book):
    """`NAME!` for a reference to another sheet, `#REF!` when that sheet is gone, else nothing."""
    if ref.sheet == OWN:
        return ""
    s = book.get(ref.sheet)
    return "#REF!" if s is None else s.name + "!"


def to_text(node, book):
    """The formula as `show` prints it (without the leading `=`)."""
    if isinstance(node, Num):
        return str(node.value)
    if isinstance(node, CellRef):
        return prefix(node.ref, book) + ref_text(node.ref)
    if isinstance(node, Func):
        if node.first is None:
            return node.name + "(#REF)"
        return node.name + "(" + prefix(node.first, book) + ref_text(node.first) + ":" + ref_text(node.last) + ")"
    if isinstance(node, Bin):
        return to_text(node.left, book) + node.op + to_text(node.right, book)
    if isinstance(node, Neg):
        return "-" + to_text(node.inner, book)
    if isinstance(node, Paren):
        return "(" + to_text(node.inner, book) + ")"
    return "#REF"


def shift_ref(ref, dcol, drow, dst_sheet):
    """The reference moved by dcol columns and drow rows (except its `$` parts) into a formula on
    dst_sheet, or None off the grid."""
    col = ref.col if ref.abs_col else ref.col + dcol
    row = ref.row if ref.abs_row else ref.row + drow
    if not cells.on_grid(col, row):
        return None
    return Ref(col, row, ref.abs_col, ref.abs_row, OWN if ref.sheet == dst_sheet else ref.sheet)


def shift(node, dcol, drow, dst_sheet):
    """A copy of the tree for a formula copied dcol columns right and drow rows down, onto dst_sheet."""
    if isinstance(node, CellRef):
        moved = shift_ref(node.ref, dcol, drow, dst_sheet)
        if moved is None:
            return BadRef()
        return CellRef(moved)
    if isinstance(node, Func):
        if node.first is None:
            return node
        first = shift_ref(node.first, dcol, drow, dst_sheet)
        last = shift_ref(node.last, dcol, drow, dst_sheet)
        if first is None or last is None:
            return Func(node.name, None, None)
        return Func(node.name, first, last)
    if isinstance(node, Bin):
        return Bin(node.op, shift(node.left, dcol, drow, dst_sheet), shift(node.right, dcol, drow, dst_sheet))
    if isinstance(node, Neg):
        return Neg(shift(node.inner, dcol, drow, dst_sheet))
    if isinstance(node, Paren):
        return Paren(shift(node.inner, dcol, drow, dst_sheet))
    return node


def qualified_refs(node, out):
    """The references of the tree, left to right (both corners of a function's range)."""
    if isinstance(node, CellRef):
        out.append(node.ref)
    elif isinstance(node, Func):
        if node.first is not None:
            out.extend([node.first, node.last])
    elif isinstance(node, Bin):
        qualified_refs(node.left, out)
        qualified_refs(node.right, out)
    elif isinstance(node, (Neg, Paren)):
        qualified_refs(node.inner, out)
    return out


def bind(node, book, own):
    """Turns the sheet names written in a freshly parsed formula on sheet `own` into sheet ids (a
    name of the own sheet into OWN). Returns the first name, from the left, that is not a sheet, or
    None."""
    for ref in qualified_refs(node, []):
        if ref.qualifier == "":
            continue
        s = book.by_name(ref.qualifier)
        if s is None:
            return ref.qualifier
    for ref in qualified_refs(node, []):
        if ref.qualifier != "":
            sid = book.by_name(ref.qualifier).id
            ref.sheet = OWN if sid == own else sid
    return None


def ref_sheet(ref, own):
    return own if ref.sheet == OWN else ref.sheet


def range_of(func):
    """(col1, row1, col2, row2) of a function's range, corners in order."""
    a, b = func.first, func.last
    return (min(a.col, b.col), min(a.row, b.row), max(a.col, b.col), max(a.row, b.row))


def referenced(node, own, out):
    """Adds to `out` every place (sheet, col, row) the tree of a formula on sheet `own` refers to,
    ranges cell by cell."""
    if isinstance(node, CellRef):
        out.append((ref_sheet(node.ref, own), node.ref.col, node.ref.row))
    elif isinstance(node, Func):
        if node.first is not None:
            sid = ref_sheet(node.first, own)
            out.extend((sid, c, r) for c, r in cells.range_cells(*range_of(node)))
    elif isinstance(node, Bin):
        referenced(node.left, own, out)
        referenced(node.right, own, out)
    elif isinstance(node, (Neg, Paren)):
        referenced(node.inner, own, out)
    return out
