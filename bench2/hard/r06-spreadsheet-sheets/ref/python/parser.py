"""Builds a formula tree from tokens.

    expr    := term (("+" | "-") term)*
    term    := unary (("*" | "/") unary)*
    unary   := "-" unary | primary
    primary := num | [sheet] ref | func "(" [sheet] ref ":" ref ")" | "(" expr ")"
"""

import lexer
from formula import Num, CellRef, Func, Bin, Neg, Paren


class ParseError(Exception):
    pass


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self):
        return self.tokens[self.pos].kind if self.pos < len(self.tokens) else ""

    def take(self, kind):
        if self.peek() != kind:
            raise ParseError()
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def expr(self):
        node = self.term()
        while self.peek() in ("+", "-"):
            op = self.take(self.peek()).kind
            node = Bin(op, node, self.term())
        return node

    def term(self):
        node = self.unary()
        while self.peek() in ("*", "/"):
            op = self.take(self.peek()).kind
            node = Bin(op, node, self.unary())
        return node

    def unary(self):
        if self.peek() == "-":
            self.take("-")
            return Neg(self.unary())
        return self.primary()

    def qualifier(self):
        return self.take("sheet").text if self.peek() == "sheet" else ""

    def primary(self):
        kind = self.peek()
        if kind == "num":
            return Num(self.take("num").value)
        if kind in ("ref", "sheet"):
            q = self.qualifier()
            ref = self.take("ref").value
            ref.qualifier = q
            return CellRef(ref)
        if kind == "func":
            name = self.take("func").text
            self.take("(")
            q = self.qualifier()
            first = self.take("ref").value
            self.take(":")
            last = self.take("ref").value
            self.take(")")
            first.qualifier = q
            last.qualifier = q
            return Func(name, first, last)
        if kind == "(":
            self.take("(")
            inner = self.expr()
            self.take(")")
            return Paren(inner)
        raise ParseError()


def parse_formula(text):
    """The tree of a formula's text (after its `=`), or None if it is not a formula."""
    tokens = lexer.tokenize(text)
    if tokens is None:
        return None
    p = Parser(tokens)
    try:
        node = p.expr()
    except ParseError:
        return None
    if p.pos != len(tokens):
        return None
    return node
