"""Cell names: a column letter A-Z and a row 1-99, such as A1 or Z99. Letters may be lower-case."""

COLS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
MAX_ROW = 99


def is_digits(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def is_int(s):
    """An optional `-` and at least one digit."""
    if s.startswith("-"):
        return is_digits(s[1:])
    return is_digits(s)


def is_sheet_name(s):
    """A letter followed by letters, digits and `_`."""
    if len(s) == 0 or not (("A" <= s[0] <= "Z") or ("a" <= s[0] <= "z")):
        return False
    return all(("A" <= c <= "Z") or ("a" <= c <= "z") or ("0" <= c <= "9") or c == "_" for c in s)


def parse_cell(text):
    """(col, row) with col 0-25 and row 1-99 for a cell name, or None."""
    if len(text) < 2 or len(text) > 3:
        return None
    letter = text[0].upper()
    digits = text[1:]
    if letter not in COLS or not is_digits(digits) or digits[0] == "0":
        return None
    return (COLS.index(letter), int(digits))


def parse_range(text):
    """(col1, row1, col2, row2) with col1 <= col2 and row1 <= row2 for `A1:C3` (corners in any order), or None."""
    parts = text.split(":")
    if len(parts) != 2:
        return None
    a = parse_cell(parts[0])
    b = parse_cell(parts[1])
    if a is None or b is None:
        return None
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1]))


def on_grid(col, row):
    return 0 <= col < len(COLS) and 1 <= row <= MAX_ROW


def cell_name(cell):
    """The name of (col, row), or of the cell part of a place (sheet, col, row)."""
    return COLS[cell[-2]] + str(cell[-1])


def range_cells(col1, row1, col2, row2):
    """The cells of a range, row by row from the top, left to right within a row."""
    out = []
    for row in range(row1, row2 + 1):
        for col in range(col1, col2 + 1):
            out.append((col, row))
    return out


def sorted_cells(cells):
    """Cells in column order, then row order (A1, A2, ..., B1), each once; places (sheet, col, row)
    by sheet first."""
    return sorted(set(cells))
