"""Tokenizer and parser: one line of text to a statement."""

KEYWORDS = set("create table int text insert into values null select from join left on where group by having "
               "order asc desc limit offset and or not is count sum min max delete update set".split())
SYMBOLS2 = ("!=", "<=", ">=")
SYMBOLS1 = "(),*.=<>"


class SyntaxErr(Exception):
    pass


def tokens(line):
    """List of (kind, value): kind is word, int, str or sym."""
    out, i, n = [], 0, len(line)
    while i < n:
        c = line[i]
        if c == " ":
            i += 1
        elif c == "_" or "a" <= c <= "z":
            j = i
            while j < n and (line[j] == "_" or "a" <= line[j] <= "z" or "0" <= line[j] <= "9"):
                j += 1
            out.append(("word", line[i:j]))
            i = j
        elif "0" <= c <= "9" or (c == "-" and i + 1 < n and "0" <= line[i + 1] <= "9"):
            j = i + 1
            while j < n and "0" <= line[j] <= "9":
                j += 1
            digits = line[i:j].lstrip("-")
            if len(digits) > 9:
                raise SyntaxErr()
            out.append(("int", line[i:j]))
            i = j
        elif c == "'":
            j, text = i + 1, ""
            while True:
                if j >= n:
                    raise SyntaxErr()
                if line[j] == "'":
                    if j + 1 < n and line[j + 1] == "'":
                        text += "'"
                        j += 2
                        continue
                    break
                text += line[j]
                j += 1
            out.append(("str", text))
            i = j + 1
        elif line[i:i + 2] in SYMBOLS2:
            out.append(("sym", line[i:i + 2]))
            i += 2
        elif c in SYMBOLS1:
            out.append(("sym", c))
            i += 1
        else:
            raise SyntaxErr()
    return out


class Col:
    def __init__(self, table, name):
        self.table, self.name, self.index = table, name, -1

    def text(self):
        return f"{self.table}.{self.name}" if self.table else self.name


class Agg:
    def __init__(self, func, arg):
        self.func, self.arg = func, arg        # arg is a Col, or None for count(*)

    def text(self):
        return f"{self.func}({self.arg.text() if self.arg else '*'})"


class Lit:
    def __init__(self, value):
        self.value = value                     # None, int or str


class Node:
    """An expression: kind is cmp, isnull, notnull, not, and, or."""
    def __init__(self, kind, a, b=None, op=None):
        self.kind, self.a, self.b, self.op = kind, a, b, op


class Stmt:
    def __init__(self, kind, **kw):
        self.kind = kind
        self.__dict__.update(kw)


class Parser:
    def __init__(self, toks):
        self.toks, self.i = toks, 0

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j] if j < len(self.toks) else (None, None)

    def at(self, value, k=0):
        t = self.peek(k)
        return t[0] in ("word", "sym") and t[1] == value

    def accept(self, value):
        if self.at(value):
            self.i += 1
            return True
        return False

    def expect(self, value):
        if not self.accept(value):
            raise SyntaxErr()

    def name(self):
        kind, v = self.peek()
        if kind != "word" or v in KEYWORDS:
            raise SyntaxErr()
        self.i += 1
        return v

    def lit(self):
        kind, v = self.peek()
        if kind in ("int", "str"):
            self.i += 1
            return Lit(int(v) if kind == "int" else v)
        self.expect("null")
        return Lit(None)

    def count(self):
        kind, v = self.peek()
        if kind != "int" or v.startswith("-"):
            raise SyntaxErr()
        self.i += 1
        return int(v)

    def col(self):
        a = self.name()
        if self.accept("."):
            return Col(a, self.name())
        return Col("", a)

    def agg(self):
        func = self.peek()[1]
        self.i += 1
        self.expect("(")
        if func == "count" and self.accept("*"):
            self.expect(")")
            return Agg(func, None)
        c = self.col()
        self.expect(")")
        return Agg(func, c)

    def is_agg(self):
        return self.peek()[0] == "word" and self.peek()[1] in ("count", "sum", "min", "max")

    def operand(self):
        if self.is_agg():
            return self.agg()
        kind, v = self.peek()
        if kind == "word" and v not in KEYWORDS:
            return self.col()
        return self.lit()

    def expr(self):
        node = self.term()
        while self.accept("or"):
            node = Node("or", node, self.term())
        return node

    def term(self):
        node = self.factor()
        while self.accept("and"):
            node = Node("and", node, self.factor())
        return node

    def factor(self):
        if self.accept("not"):
            return Node("not", self.factor())
        if self.accept("("):
            node = self.expr()
            self.expect(")")
            return node
        a = self.operand()
        if self.accept("is"):
            if self.accept("not"):
                self.expect("null")
                return Node("notnull", a)
            self.expect("null")
            return Node("isnull", a)
        kind, op = self.peek()
        if kind != "sym" or op not in ("=", "!=", "<", "<=", ">", ">="):
            raise SyntaxErr()
        self.i += 1
        return Node("cmp", a, self.operand(), op)

    def statement(self):
        if self.accept("create"):
            self.expect("table")
            t = self.name()
            self.expect("(")
            cols = []
            while True:
                c = self.name()
                kind, ty = self.peek()
                if not (self.accept("int") or self.accept("text")):
                    raise SyntaxErr()
                cols.append((c, ty))
                if not self.accept(","):
                    break
            self.expect(")")
            return Stmt("create", table=t, cols=cols)
        if self.accept("insert"):
            self.expect("into")
            t = self.name()
            self.expect("values")
            tuples = []
            while True:
                self.expect("(")
                vals = [self.lit()]
                while self.accept(","):
                    vals.append(self.lit())
                self.expect(")")
                tuples.append(vals)
                if not self.accept(","):
                    break
            return Stmt("insert", table=t, tuples=tuples)
        if self.accept("delete"):
            self.expect("from")
            t = self.name()
            return Stmt("delete", table=t, where=self.expr() if self.accept("where") else None)
        if self.accept("update"):
            t = self.name()
            self.expect("set")
            sets = []
            while True:
                c = self.name()
                self.expect("=")
                sets.append((c, self.lit()))
                if not self.accept(","):
                    break
            return Stmt("update", table=t, sets=sets, where=self.expr() if self.accept("where") else None)
        self.expect("select")
        return self.select()

    def select(self):
        items = None
        if not self.accept("*"):
            items = [self.agg() if self.is_agg() else self.col()]
            while self.accept(","):
                items.append(self.agg() if self.is_agg() else self.col())
        self.expect("from")
        s = Stmt("select", items=items, table=self.name(), join=None, left=False, on=None, where=None,
                 group=None, having=None, order=[], limit=None, offset=0)
        if self.at("join") or (self.at("left") and self.at("join", 1)):
            s.left = self.accept("left")
            self.expect("join")
            s.join = self.name()
            self.expect("on")
            a = self.col()
            self.expect("=")
            s.on = Node("cmp", a, self.col(), "=")
        if self.accept("where"):
            s.where = self.expr()
        if self.accept("group"):
            self.expect("by")
            s.group = [self.col()]
            while self.accept(","):
                s.group.append(self.col())
        if self.accept("having"):
            s.having = self.expr()
        if self.accept("order"):
            self.expect("by")
            while True:
                k = self.agg() if self.is_agg() else self.col()
                desc = False
                if self.accept("desc"):
                    desc = True
                else:
                    self.accept("asc")
                s.order.append((k, desc))
                if not self.accept(","):
                    break
        if self.accept("limit"):
            s.limit = self.count()
            if self.accept("offset"):
                s.offset = self.count()
        return s


def parse(line):
    """A Stmt, or raises SyntaxErr."""
    p = Parser(tokens(line))
    s = p.statement()
    if p.i != len(p.toks):
        raise SyntaxErr()
    return s
