"""The cells of the sheet. A cell holds a number or a formula; a cell that holds nothing is empty."""

import cells


class Cell:
    def __init__(self, number, node):
        self.number = number    # the number, when node is None
        self.node = node        # the formula tree, or None


class Sheet:
    def __init__(self):
        self.cells = {}

    def get(self, cell):
        """The Cell at (col, row), or None when it is empty."""
        return self.cells.get(cell)

    def put_number(self, cell, number):
        self.cells[cell] = Cell(number, None)

    def put_formula(self, cell, node):
        self.cells[cell] = Cell(0, node)

    def clear(self, cell):
        del self.cells[cell]

    def filled(self):
        """The non-empty cells, sorted."""
        return cells.sorted_cells(self.cells.keys())

    def formula_cells(self):
        return [c for c in self.filled() if self.cells[c].node is not None]
