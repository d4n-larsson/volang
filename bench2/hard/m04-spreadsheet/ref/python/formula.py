"""Formula language: tokens, parsing into a tree, and reference shifting for copy."""

AGGREGATES = ("SUM", "MIN", "MAX", "COUNT")
FUNCTIONS = AGGREGATES + ("IF", "ABS")
WORD = set("$ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def parse_ref(w):
    """(col_fixed, col, row_fixed, row) for a reference word, or None."""
    i = 0
    cf = w[:1] == "$"
    if cf:
        i = 1
    if i >= len(w) or w[i] not in LETTERS:
        return None
    col = LETTERS.index(w[i])
    i += 1
    rf = w[i:i + 1] == "$"
    if rf:
        i += 1
    digits = w[i:]
    if not digits.isdigit() or not digits.isascii() or digits[0] == "0" or len(digits) > 2:
        return None
    return (cf, col, rf, int(digits))


def ref_text(r):
    cf, col, rf, row = r
    return ("$" if cf else "") + LETTERS[col] + ("$" if rf else "") + str(row)


def read_word(s, i):
    j = i
    while j < len(s) and s[j] in WORD:
        j += 1
    return s[i:j], j


def tokenize(s):
    """List of (kind, start, end, data), or None if the text has an invalid token."""
    toks = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == " ":
            i += 1
        elif c.isdigit() and c.isascii():
            j = i
            while j < len(s) and s[j].isdigit() and s[j].isascii():
                j += 1
            toks.append(("num", i, j, int(s[i:j])))
            i = j
        elif c == "$" or c in LETTERS:
            w, j = read_word(s, i)
            r = parse_ref(w)
            if r is not None:
                if j < len(s) and s[j] == ":":
                    if j + 1 < len(s) and (s[j + 1] == "$" or s[j + 1] in LETTERS):
                        w2, k = read_word(s, j + 1)
                        r2 = parse_ref(w2)
                        if r2 is None:
                            return None
                        toks.append(("range", i, k, (r, r2)))
                        i = k
                        continue
                    return None
                toks.append(("ref", i, j, r))
            elif len(w) >= 2 and all(ch in LETTERS for ch in w):
                toks.append(("name", i, j, w))
            else:
                return None
            i = j
        elif s.startswith("#REF!", i):
            toks.append(("referr", i, i + 5, None))
            i += 5
        elif s[i:i + 2] in ("<=", ">=", "<>"):
            toks.append(("op", i, i + 2, s[i:i + 2]))
            i += 2
        elif c in "+-*/=<>(),":
            toks.append(("op", i, i + 1, c))
            i += 1
        else:
            return None
    return toks


class Bad(Exception):
    pass


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j] if j < len(self.toks) else None

    def is_op(self, t, ops):
        return t is not None and t[0] == "op" and t[3] in ops

    def expect(self, op):
        t = self.peek()
        if not self.is_op(t, (op,)):
            raise Bad()
        self.i += 1

    def expression(self):
        left = self.sum()
        t = self.peek()
        if self.is_op(t, ("=", "<>", "<", "<=", ">", ">=")):
            self.i += 1
            left = ("bin", t[3], left, self.sum())
        return left

    def sum(self):
        left = self.product()
        while self.is_op(self.peek(), ("+", "-")):
            op = self.peek()[3]
            self.i += 1
            left = ("bin", op, left, self.product())
        return left

    def product(self):
        left = self.unary()
        while self.is_op(self.peek(), ("*", "/")):
            op = self.peek()[3]
            self.i += 1
            left = ("bin", op, left, self.unary())
        return left

    def unary(self):
        if self.is_op(self.peek(), ("-",)):
            self.i += 1
            return ("neg", self.unary())
        return self.primary()

    def primary(self):
        t = self.peek()
        if t is None:
            raise Bad()
        self.i += 1
        if t[0] == "num":
            return ("num", t[3])
        if t[0] == "ref":
            return ("ref", t[3][1], t[3][3])
        if t[0] == "referr":
            return ("referr",)
        if self.is_op(t, ("(",)):
            e = self.expression()
            self.expect(")")
            return e
        if t[0] == "name":
            return self.call(t[3])
        raise Bad()

    def call(self, name):
        if name not in FUNCTIONS:
            raise Bad()
        self.expect("(")
        args = [self.argument(name)]
        while self.is_op(self.peek(), (",",)):
            self.i += 1
            args.append(self.argument(name))
        self.expect(")")
        if (name == "IF" and len(args) != 3) or (name == "ABS" and len(args) != 1):
            raise Bad()
        return ("call", name, args)

    def argument(self, name):
        t = self.peek()
        if name in AGGREGATES and t is not None and t[0] in ("ref", "range") \
                and self.is_op(self.peek(1), (",", ")")):
            self.i += 1
            a, b = (t[3], t[3]) if t[0] == "ref" else t[3]
            return ("range", min(a[1], b[1]), min(a[3], b[3]), max(a[1], b[1]), max(a[3], b[3]))
        return self.expression()


def parse(text):
    """The tree of a formula's expression (the text after `=`), or None if it is invalid."""
    toks = tokenize(text)
    if toks is None:
        return None
    p = Parser(toks)
    try:
        e = p.expression()
    except Bad:
        return None
    if p.i != len(toks):
        return None
    return e


def move(r, dc, dr):
    cf, col, rf, row = r
    if not cf:
        col += dc
    if not rf:
        row += dr
    if col < 0 or col > 25 or row < 1 or row > 99:
        return None
    return (cf, col, rf, row)


def shift(text, dc, dr):
    """The formula expression text with its references moved for a copy."""
    out = []
    last = 0
    for kind, a, b, data in tokenize(text):
        if kind == "ref":
            m = move(data, dc, dr)
            new = "#REF!" if m is None else ref_text(m)
        elif kind == "range":
            m1, m2 = move(data[0], dc, dr), move(data[1], dc, dr)
            new = "#REF!" if m1 is None or m2 is None else ref_text(m1) + ":" + ref_text(m2)
        else:
            continue
        out.append(text[last:a])
        out.append(new)
        last = b
    out.append(text[last:])
    return "".join(out)
