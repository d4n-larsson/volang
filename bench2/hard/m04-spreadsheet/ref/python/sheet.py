"""The sheet: contents, dependencies, cycles and evaluation."""
import formula

LIMIT = 999999999


def num(v):
    return ("n", v) if -LIMIT <= v <= LIMIT else ("e", "#NUM!")


def div(a, b):
    q = (2 * abs(a) + abs(b)) // (2 * abs(b))
    return q if (a < 0) == (b < 0) else -q


class Sheet:
    def __init__(self):
        self.cells = {}   # (col, row) -> ("num", v) | ("text", s) | ("formula", text, tree or None)
        self.memo = {}
        self.cyc = None

    def put(self, cell, content):
        self.memo, self.cyc = {}, None
        if content is None:
            self.cells.pop(cell, None)
        else:
            self.cells[cell] = content

    # ---- dependencies ----

    def direct(self, cell):
        c = self.cells.get(cell)
        if c is None or c[0] != "formula" or c[2] is None:
            return set()
        out = set()
        collect(c[2], out)
        return out

    def cycles(self):
        if self.cyc is None:
            graph = {cell: self.direct(cell) for cell in self.cells}
            self.cyc = set()
            for cell in graph:
                seen, stack = set(), list(graph[cell])
                while stack:
                    x = stack.pop()
                    if x == cell:
                        self.cyc.add(cell)
                        break
                    if x not in seen:
                        seen.add(x)
                        stack.extend(graph.get(x, ()))
        return self.cyc

    def dependents(self, target):
        graph = {cell: self.direct(cell) for cell in self.cells}
        found, todo = set(), [target]
        while todo:
            t = todo.pop()
            for cell, ds in graph.items():
                if t in ds and cell not in found:
                    found.add(cell)
                    todo.append(cell)
        return sorted(found, key=lambda c: (c[1], c[0]))

    # ---- values ----

    def value(self, cell):
        """("n", int) | ("t", str) | ("e", code) | None for an empty cell."""
        if cell in self.memo:
            return self.memo[cell]
        c = self.cells.get(cell)
        if c is None:
            v = None
        elif c[0] == "num":
            v = num(c[1])
        elif c[0] == "text":
            v = ("t", c[1])
        elif c[2] is None:
            v = ("e", "#PARSE!")
        elif cell in self.cycles():
            v = ("e", "#CYCLE!")
        else:
            v = self.eval(c[2])
        self.memo[cell] = v
        return v

    def eval(self, e):
        k = e[0]
        if k == "num":
            return num(e[1])
        if k == "referr":
            return ("e", "#REF!")
        if k == "ref":
            v = self.value((e[1], e[2]))
            return ("n", 0) if v is None else v
        if k == "neg":
            v = self.eval(e[1])
            if v[0] == "e":
                return v
            if v[0] == "t":
                return ("e", "#VALUE!")
            return ("n", -v[1])
        if k == "bin":
            a = self.eval(e[2])
            if a[0] == "e":
                return a
            b = self.eval(e[3])
            if b[0] == "e":
                return b
            if a[0] == "t" or b[0] == "t":
                return ("e", "#VALUE!")
            return binop(e[1], a[1], b[1])
        return self.call(e[1], e[2])

    def call(self, name, args):
        if name == "IF":
            c = self.eval(args[0])
            if c[0] == "e":
                return c
            if c[0] == "t":
                return ("e", "#VALUE!")
            return self.eval(args[1] if c[1] != 0 else args[2])
        if name == "ABS":
            v = self.eval(args[0])
            if v[0] == "e":
                return v
            if v[0] == "t":
                return ("e", "#VALUE!")
            return ("n", abs(v[1]))
        nums = []
        for a in args:
            if a[0] == "range":
                for row in range(a[2], a[4] + 1):
                    for col in range(a[1], a[3] + 1):
                        v = self.value((col, row))
                        if v is None or v[0] == "t":
                            continue
                        if v[0] == "e":
                            return v
                        nums.append(v[1])
            else:
                v = self.eval(a)
                if v[0] == "e":
                    return v
                if v[0] == "t":
                    return ("e", "#VALUE!")
                nums.append(v[1])
        if name == "SUM":
            return num(sum(nums))
        if name == "COUNT":
            return ("n", len(nums))
        if not nums:
            return ("n", 0)
        return ("n", min(nums) if name == "MIN" else max(nums))


def binop(op, a, b):
    if op == "+":
        return num(a + b)
    if op == "-":
        return num(a - b)
    if op == "*":
        return num(a * b)
    if op == "/":
        return ("e", "#DIV/0!") if b == 0 else num(div(a, b))
    r = {"=": a == b, "<>": a != b, "<": a < b, "<=": a <= b, ">": a > b, ">=": a >= b}[op]
    return ("n", 1 if r else 0)


def collect(e, out):
    k = e[0]
    if k == "ref":
        out.add((e[1], e[2]))
    elif k == "range":
        for row in range(e[2], e[4] + 1):
            for col in range(e[1], e[3] + 1):
                out.add((col, row))
    elif k == "neg":
        collect(e[1], out)
    elif k == "bin":
        collect(e[2], out)
        collect(e[3], out)
    elif k == "call":
        for a in e[2]:
            collect(a, out)


def content_of(text):
    """The stored content for the text given to `set`."""
    body = text[1:] if text.startswith("-") else text
    if body.isdigit() and body.isascii():
        return ("num", int(text))
    if text.startswith("="):
        return ("formula", text, formula.parse(text[1:]))
    return ("text", text)
