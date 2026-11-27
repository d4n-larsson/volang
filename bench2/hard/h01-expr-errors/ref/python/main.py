import sys


class Problem(Exception):
    def __init__(self, col, msg):
        self.col, self.msg = col, msg


class Parser:
    def __init__(self, line):
        self.s = line
        self.i = 0
        self.tok = None
        self.advance()

    def advance(self):
        s, i = self.s, self.i
        while i < len(s) and s[i] in " \t":
            i += 1
        if i == len(s):
            self.i = i
            self.tok = ("end", "", len(s) + 1)
            return
        c = s[i]
        if c.isascii() and c.isdigit():
            j = i
            while j < len(s) and s[j].isascii() and s[j].isdigit():
                j += 1
            self.tok = ("num", s[i:j], i + 1)
        elif "a" <= c <= "z":
            j = i
            while j < len(s) and "a" <= s[j] <= "z":
                j += 1
            name = s[i:j]
            if name not in ("min", "max", "abs"):
                raise Problem(i + 1, "unknown function " + name)
            self.tok = ("name", name, i + 1)
        elif c in "+-*/%^(),":
            j = i + 1
            self.tok = ("op", c, i + 1)
        else:
            raise Problem(i + 1, "unexpected character " + c)
        self.i = j

    def fail(self):
        kind, text, col = self.tok
        if kind == "end":
            raise Problem(col, "unexpected end of line")
        raise Problem(col, "unexpected " + text)

    def expect(self, text):
        if self.tok[1] != text or self.tok[0] == "end":
            self.fail()
        self.advance()

    # Nodes: ("num", v) | ("bin", op, col, l, r) | ("neg", x) | ("call", name, args)
    def expr(self):
        node = self.term()
        while self.tok[0] == "op" and self.tok[1] in "+-":
            op, col = self.tok[1], self.tok[2]
            self.advance()
            node = ("bin", op, col, node, self.term())
        return node

    def term(self):
        node = self.unary()
        while self.tok[0] == "op" and self.tok[1] in "*/%":
            op, col = self.tok[1], self.tok[2]
            self.advance()
            node = ("bin", op, col, node, self.unary())
        return node

    def unary(self):
        if self.tok[0] == "op" and self.tok[1] == "-":
            self.advance()
            return ("neg", self.unary())
        return self.power()

    def power(self):
        base = self.primary()
        if self.tok[0] == "op" and self.tok[1] == "^":
            col = self.tok[2]
            self.advance()
            return ("bin", "^", col, base, self.unary())
        return base

    def primary(self):
        kind, text, col = self.tok
        if kind == "num":
            self.advance()
            return ("num", int(text))
        if kind == "op" and text == "(":
            self.advance()
            node = self.expr()
            self.expect(")")
            return node
        if kind == "name":
            self.advance()
            self.expect("(")
            args = [self.expr()]
            while text != "abs" and self.tok[0] == "op" and self.tok[1] == ",":
                self.advance()
                args.append(self.expr())
            self.expect(")")
            return ("call", text, args)
        self.fail()


def ev(node):
    k = node[0]
    if k == "num":
        return node[1]
    if k == "neg":
        return -ev(node[1])
    if k == "call":
        vals = [ev(a) for a in node[2]]
        if node[1] == "min":
            return min(vals)
        if node[1] == "max":
            return max(vals)
        return abs(vals[0])
    _, op, col, l, r = node
    a = ev(l)
    b = ev(r)
    if op == "+":
        return a + b
    if op == "-":
        return a - b
    if op == "*":
        return a * b
    if op in "/%":
        if b == 0:
            raise Problem(col, "division by zero")
        rem = a % abs(b)
        return rem if op == "%" else (a - rem) // b
    if b < 0:
        raise Problem(col, "negative exponent")
    return a ** b


def run(line):
    try:
        p = Parser(line)
        node = p.expr()
        if p.tok[0] != "end":
            p.fail()
        return str(ev(node))
    except Problem as e:
        return f"error at column {e.col}: {e.msg}"


def main():
    data = sys.stdin.buffer.read().decode("utf-8")
    if data == "":
        return
    lines = data.split("\n")
    if data.endswith("\n"):
        lines.pop()
    sys.stdout.write("".join(run(l) + "\n" for l in lines))


main()
