from lexer import SyntaxErr

# Trees: ("num", digits) ("name", s) ("neg", e) ("bin", op, l, r)


class Parser:
    def __init__(self, toks):
        self.toks, self.i = toks, 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def take_op(self, ops):
        k, v = self.peek()
        if k == "op" and v in ops:
            self.i += 1
            return v
        return None

    def expr(self):
        e = self.term()
        while (op := self.take_op("+-")):
            e = ("bin", op, e, self.term())
        return e

    def term(self):
        e = self.unary()
        while (op := self.take_op("*/%")):
            e = ("bin", op, e, self.unary())
        return e

    def unary(self):
        if self.take_op("-"):
            return ("neg", self.unary())
        return self.power()

    def power(self):
        a = self.atom()
        if self.take_op("^"):
            return ("bin", "^", a, self.unary())
        return a

    def atom(self):
        k, v = self.peek()
        if k in ("num", "name"):
            self.i += 1
            return (k, v)
        if self.take_op("("):
            e = self.expr()
            if not self.take_op(")"):
                raise SyntaxErr()
            return e
        raise SyntaxErr()


def parse_line(toks):
    """(name or None, tree); raises SyntaxErr."""
    name = None
    if len(toks) >= 2 and toks[0][0] == "name" and toks[1] == ("op", "="):
        name, toks = toks[0][1], toks[2:]
    p = Parser(toks)
    e = p.expr()
    if p.i != len(toks):
        raise SyntaxErr()
    return name, e
