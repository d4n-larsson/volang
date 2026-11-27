import re
import sys

REF = re.compile(r"[A-Z][1-9][0-9]{0,3}")
NUMBER = re.compile(r"-?[0-9]{1,18}")


class ParseError(Exception):
    pass


def ref_pos(r):
    return int(r[1:]), ord(r[0]) - 65          # (row, column)


def tokenize(s):
    toks, i = [], 0
    while i < len(s):
        c = s[i]
        if c == " ":
            i += 1
        elif c in "+-*(),:":
            toks.append(c)
            i += 1
        elif c.isascii() and c.isalnum():
            j = i
            while j < len(s) and s[j].isascii() and s[j].isalnum():
                j += 1
            toks.append(s[i:j])
            i = j
        else:
            raise ParseError()
    return toks


class Parser:
    def __init__(self, toks):
        self.t, self.i = toks, 0

    def peek(self, k=0):
        return self.t[self.i + k] if self.i + k < len(self.t) else None

    def take(self, want=None):
        tok = self.peek()
        if tok is None or (want is not None and tok != want):
            raise ParseError()
        self.i += 1
        return tok

    def expr(self):
        node = self.term()
        while self.peek() in ("+", "-"):
            op = self.take()
            node = ("bin", op, node, self.term())
        return node

    def term(self):
        node = self.unary()
        while self.peek() == "*":
            self.take()
            node = ("bin", "*", node, self.unary())
        return node

    def unary(self):
        if self.peek() == "-":
            self.take()
            return ("neg", self.unary())
        return self.primary()

    def primary(self):
        tok = self.take()
        if tok == "(":
            node = self.expr()
            self.take(")")
            return node
        if re.fullmatch(r"[0-9]{1,18}", tok):
            return ("num", int(tok))
        if REF.fullmatch(tok):
            return ("ref", ref_pos(tok))
        if tok in ("SUM", "MIN", "MAX"):
            self.take("(")
            args = [self.arg()]
            while self.peek() == ",":
                self.take()
                args.append(self.arg())
            self.take(")")
            return ("call", tok, args)
        raise ParseError()

    def arg(self):
        a, b = self.peek(), self.peek(1)
        if a is not None and REF.fullmatch(a) and b == ":":
            self.take()
            self.take()
            c = self.take()
            if not REF.fullmatch(c):
                raise ParseError()
            (r1, c1), (r2, c2) = ref_pos(a), ref_pos(c)
            cells = [(r, k) for r in range(min(r1, r2), max(r1, r2) + 1)
                     for k in range(min(c1, c2), max(c1, c2) + 1)]
            return ("range", cells)
        return self.expr()


def refs(node, out):
    k = node[0]
    if k == "ref":
        out.append(node[1])
    elif k == "range":
        out.extend(node[1])
    elif k == "neg":
        refs(node[1], out)
    elif k == "bin":
        refs(node[2], out)
        refs(node[3], out)
    elif k == "call":
        for a in node[2]:
            refs(a, out)


class Err(Exception):
    def __init__(self, code):
        self.code = code


def main():
    cells = {}
    for line in sys.stdin.read().split("\n"):
        if line:
            ref, content = line.split(" ", 1)
            cells[ref_pos(ref)] = content
    kind = {}
    for pos, content in cells.items():
        if content.startswith("="):
            try:
                p = Parser(tokenize(content[1:]))
                node = p.expr()
                if p.peek() is not None:
                    raise ParseError()
                kind[pos] = ("formula", node)
            except ParseError:
                kind[pos] = ("error", "#PARSE")
        elif NUMBER.fullmatch(content):
            kind[pos] = ("num", int(content))
        else:
            kind[pos] = ("text", content)
    deps = {}
    for pos, k in kind.items():
        if k[0] == "formula":
            out = []
            refs(k[1], out)
            deps[pos] = out
    # Tarjan's strongly connected components; a path visits at most 60 cells.
    index, low, stack, on, counter, cyclic = {}, {}, [], set(), [0], set()

    def visit(v):
        index[v] = low[v] = counter[0]
        counter[0] += 1
        stack.append(v)
        on.add(v)
        for w in deps.get(v, ()):
            if w not in index:
                visit(w)
                low[v] = min(low[v], low[w])
            elif w in on:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            comp = []
            while True:
                w = stack.pop()
                on.discard(w)
                comp.append(w)
                if w == v:
                    break
            if len(comp) > 1 or v in deps.get(v, ()):
                cyclic.update(comp)

    for v in deps:
        if v not in index:
            visit(v)

    memo = {}

    def value(pos):
        """int, str (text) or Err; empty cells are None."""
        if pos in memo:
            return memo[pos]
        k = kind.get(pos)
        if k is None:
            v = None
        elif pos in cyclic:
            v = Err("#CYCLE")
        elif k[0] == "formula":
            try:
                v = ev(k[1])
            except Err as e:
                v = e
        elif k[0] == "error":
            v = Err(k[1])
        else:
            v = k[1]
        memo[pos] = v
        return v

    def as_int(pos):
        v = value(pos)
        if v is None:
            return 0
        if isinstance(v, Err):
            raise v
        if isinstance(v, str):
            raise Err("#VALUE")
        return v

    def ev(node):
        k = node[0]
        if k == "num":
            return node[1]
        if k == "ref":
            return as_int(node[1])
        if k == "neg":
            return -ev(node[1])
        if k == "bin":
            a = ev(node[2])
            b = ev(node[3])
            return a + b if node[1] == "+" else a - b if node[1] == "-" else a * b
        vals = []
        for a in node[2]:
            if a[0] == "range":
                for pos in a[1]:
                    v = value(pos)
                    if isinstance(v, Err):
                        raise v
                    if isinstance(v, int):
                        vals.append(v)
            else:
                vals.append(ev(a))
        if node[1] == "SUM":
            return sum(vals)
        if not vals:
            raise Err("#VALUE")
        return min(vals) if node[1] == "MIN" else max(vals)

    out = []
    for pos in sorted(kind):
        v = value(pos)
        shown = v.code if isinstance(v, Err) else str(v)
        out.append(f"{chr(65 + pos[1])}{pos[0]} {shown}\n")
    sys.stdout.write("".join(out))


main()
