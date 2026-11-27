"""Parses expressions, statements and the block structure."""
from lexer import SyntaxErr, tokenize

BUILTINS = {"len": 1, "str": 1, "int": 1, "substr": 3}
CMP = ("==", "!=", "<", "<=", ">", ">=")


class ParseError(Exception):
    def __init__(self, line, msg):
        self.line, self.msg = line, msg


class Expr:
    """Parser for the tokens of one line; pos moves through them."""

    def __init__(self, toks):
        self.toks, self.pos = toks, 0

    def peek(self, k=0):
        i = self.pos + k
        return self.toks[i] if i < len(self.toks) else (None, None)

    def take(self, kind=None, text=None):
        t = self.peek()
        if t[0] is None or (kind and t[0] != kind) or (text and t[1] != text):
            raise SyntaxErr()
        self.pos += 1
        return t

    def at(self, kind, text):
        return self.peek() == (kind, text)

    def done(self):
        return self.pos == len(self.toks)

    def expr(self):
        left = self.and_expr()
        while self.at("kw", "or"):
            self.pos += 1
            left = ("or", left, self.and_expr())
        return left

    def and_expr(self):
        left = self.not_expr()
        while self.at("kw", "and"):
            self.pos += 1
            left = ("and", left, self.not_expr())
        return left

    def not_expr(self):
        if self.at("kw", "not"):
            self.pos += 1
            return ("not", self.not_expr())
        return self.cmp()

    def cmp(self):
        left = self.sum()
        t = self.peek()
        if t[0] == "op" and t[1] in CMP:
            self.pos += 1
            return ("bin", t[1], left, self.sum())
        return left

    def sum(self):
        left = self.term()
        while self.peek()[0] == "op" and self.peek()[1] in "+-":
            op = self.take()[1]
            left = ("bin", op, left, self.term())
        return left

    def term(self):
        left = self.unary()
        while self.peek()[0] == "op" and self.peek()[1] in ("*", "/", "%"):
            op = self.take()[1]
            left = ("bin", op, left, self.unary())
        return left

    def unary(self):
        if self.at("op", "-"):
            self.pos += 1
            return ("neg", self.unary())
        return self.primary()

    def primary(self):
        kind, text = self.take()
        if kind == "int":
            return ("val", int(text))
        if kind == "str":
            return ("val", text)
        if kind == "kw" and text in ("true", "false"):
            return ("val", text == "true")
        if kind == "name":
            if self.at("op", "("):
                return self.call(text)
            return ("var", text)
        if (kind, text) == ("op", "("):
            e = self.expr()
            self.take("op", ")")
            return e
        raise SyntaxErr()

    def call(self, name):
        self.take("op", "(")
        args = []
        if not self.at("op", ")"):
            args.append(self.expr())
            while self.at("op", ","):
                self.pos += 1
                args.append(self.expr())
        self.take("op", ")")
        return ("call", name, args)


def parse_line(toks):
    """One statement as a dict (kind, plus its parts), or a SyntaxErr."""
    p = Expr(toks)
    kind, text = p.take()
    st = {}
    if kind == "kw" and text in ("let", "set"):
        st = {"kind": text, "name": p.take("name")[1]}
        p.take("op", "=")
        st["expr"] = p.expr()
    elif (kind, text) == ("kw", "print"):
        st = {"kind": "print", "exprs": [p.expr()]}
        while p.at("op", ","):
            p.pos += 1
            st["exprs"].append(p.expr())
    elif kind == "kw" and text in ("if", "elif", "while"):
        st = {"kind": text, "expr": p.expr()}
    elif kind == "kw" and text in ("else", "end", "break", "continue"):
        st = {"kind": text}
    elif (kind, text) == ("kw", "return"):
        st = {"kind": "return", "expr": None if p.done() else p.expr()}
    elif (kind, text) == ("kw", "func"):
        st = {"kind": "func", "name": p.take("name")[1], "params": []}
        p.take("op", "(")
        if not p.at("op", ")"):
            st["params"].append(p.take("name")[1])
            while p.at("op", ","):
                p.pos += 1
                st["params"].append(p.take("name")[1])
        p.take("op", ")")
    elif kind == "name" and p.at("op", "("):
        st = {"kind": "call", "expr": p.call(text)}
    else:
        raise SyntaxErr()
    if not p.done():
        raise SyntaxErr()
    return st


def parse_program(text):
    """(top-level statements, functions by name) or a ParseError."""
    top = []
    funcs = {}
    stack = []      # open blocks: dicts with kind, line, body (list being filled), stmt
    for n, line in enumerate(text.split("\n"), 1):
        try:
            toks = tokenize(line)
            if not toks:
                continue
            st = parse_line(toks)
        except SyntaxErr:
            raise ParseError(n, "syntax error")
        st["line"] = n
        k = st["kind"]
        body = stack[-1]["body"] if stack else top
        if k == "end":
            if not stack:
                raise ParseError(n, "unexpected end")
            stack.pop()
        elif k in ("elif", "else"):
            b = stack[-1] if stack else None
            if not b or b["kind"] != "if" or b["stmt"]["else"] is not None:
                raise ParseError(n, "unexpected " + k)
            if k == "elif":
                b["stmt"]["parts"].append((st["expr"], n, []))
                b["body"] = b["stmt"]["parts"][-1][2]
            else:
                b["stmt"]["else"] = []
                b["body"] = b["stmt"]["else"]
        elif k == "func":
            if stack:
                raise ParseError(n, "nested function")
            if st["name"] in funcs or st["name"] in BUILTINS:
                raise ParseError(n, "duplicate function " + st["name"])
            for i, a in enumerate(st["params"]):
                if a in st["params"][:i]:
                    raise ParseError(n, "duplicate parameter " + a)
            st["body"] = []
            funcs[st["name"]] = st
            stack.append({"kind": "func", "line": n, "body": st["body"], "stmt": st})
        elif k == "if":
            node = {"kind": "if", "line": n, "parts": [(st["expr"], n, [])], "else": None}
            body.append(node)
            stack.append({"kind": "if", "line": n, "body": node["parts"][0][2], "stmt": node})
        elif k == "while":
            st["body"] = []
            body.append(st)
            stack.append({"kind": "while", "line": n, "body": st["body"], "stmt": st})
        else:
            if k == "return" and not any(b["kind"] == "func" for b in stack):
                raise ParseError(n, "return outside function")
            if k in ("break", "continue") and not any(b["kind"] == "while" for b in stack):
                raise ParseError(n, k + " outside loop")
            body.append(st)
    if stack:
        raise ParseError(stack[-1]["line"], "missing end")
    return top, funcs
