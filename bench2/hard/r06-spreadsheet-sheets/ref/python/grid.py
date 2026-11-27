"""The `print` grid."""

import cells
import evaluate


def pad(text, width):
    """text right-aligned in width characters."""
    return " " * (width - len(text)) + text


def render(book, sheet_id, col1, row1, col2, row2):
    """A header line with the column letters, then one line per row: the row number, then each cell's
    value right-aligned in its column (empty cells blank). Columns are as wide as their widest entry and
    separated by two spaces; the row numbers are as wide as the last one. Lines lose trailing spaces."""
    texts = {}
    for cell in cells.range_cells(col1, row1, col2, row2):
        place = (sheet_id,) + cell
        if book.cell(place) is not None:
            texts[cell] = evaluate.value_text(evaluate.value_of(book, place))
    label_width = len(str(row2))
    widths = {}
    for col in range(col1, col2 + 1):
        widths[col] = max([1] + [len(texts[(col, r)]) for r in range(row1, row2 + 1) if (col, r) in texts])
    header = " " * label_width
    for col in range(col1, col2 + 1):
        header += "  " + pad(cells.COLS[col], widths[col])
    lines = [header.rstrip(" ")]
    for row in range(row1, row2 + 1):
        line = pad(str(row), label_width)
        for col in range(col1, col2 + 1):
            line += "  " + pad(texts.get((col, row), ""), widths[col])
        lines.append(line.rstrip(" "))
    return lines
