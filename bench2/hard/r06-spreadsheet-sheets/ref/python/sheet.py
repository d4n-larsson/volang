"""The sheets of the book. A cell holds a number or a formula; a cell that holds nothing is empty.

Sheets have ids that are never reused, in the order the sheets were made; a place is
(sheet id, col, row).
"""

import cells


class Cell:
    def __init__(self, number, node):
        self.number = number    # the number, when node is None
        self.node = node        # the formula tree, or None


class Sheet:
    def __init__(self, sheet_id, name):
        self.id = sheet_id
        self.name = name
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


class Book:
    def __init__(self):
        self.sheets = {}    # id -> Sheet, the live sheets
        self.next_id = 1
        self.current = self.add("Sheet1").id

    def add(self, name):
        s = Sheet(self.next_id, name)
        self.sheets[s.id] = s
        self.next_id += 1
        return s

    def get(self, sheet_id):
        """The live sheet with this id, or None."""
        return self.sheets.get(sheet_id)

    def by_name(self, name):
        for s in self.sheets.values():
            if s.name == name:
                return s
        return None

    def drop(self, sheet_id):
        del self.sheets[sheet_id]

    def in_order(self):
        return [self.sheets[i] for i in sorted(self.sheets)]

    def cell(self, place):
        """The Cell at a place, or None when it is empty or its sheet is gone."""
        s = self.get(place[0])
        return None if s is None else s.get((place[1], place[2]))
