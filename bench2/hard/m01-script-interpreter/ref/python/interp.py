"""Runs a parsed program: values, scopes, calls and runtime errors."""
from parser import BUILTINS

LIMIT = 10 ** 18
MAX_STEPS = 10000
MAX_DEPTH = 25


class RunError(Exception):
    def __init__(self, msg):
        self.msg, self.line = msg, None


class Break(Exception):
    pass


class Continue(Exception):
    pass


class Return(Exception):
    def __init__(self, value):
        self.value = value


def show(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def kind(v):
    return "bool" if isinstance(v, bool) else "int" if isinstance(v, int) else "str"


def checked(v):
    if v < -LIMIT or v > LIMIT:
        raise RunError("overflow")
    return v


def trunc_div(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def binary(op, a, b):
    ka, kb = kind(a), kind(b)
    if op in ("==", "!="):
        same = ka == kb and a == b
        return same if op == "==" else not same
    if op in ("<", "<=", ">", ">="):
        if ka != kb or ka == "bool":
            raise RunError("type error")
        return {"<": a < b, "<=": a <= b, ">": a > b, ">=": a >= b}[op]
    if op == "+" and ka == kb == "str":
        return a + b
    if ka != "int" or kb != "int":
        raise RunError("type error")
    if op in ("/", "%") and b == 0:
        raise RunError("division by zero")
    if op == "+":
        return checked(a + b)
    if op == "-":
        return checked(a - b)
    if op == "*":
        return checked(a * b)
    q = trunc_div(a, b)
    return q if op == "/" else a - b * q


def builtin(name, args):
    if name == "len":
        if kind(args[0]) != "str":
            raise RunError("type error")
        return len(args[0])
    if name == "str":
        return show(args[0])
    if name == "int":
        v = args[0]
        if kind(v) == "int":
            return v
        if kind(v) == "bool":
            raise RunError("type error")
        d = v[1:] if v.startswith("-") else v
        if not (1 <= len(d) <= 18 and all("0" <= c <= "9" for c in d)):
            raise RunError("bad int")
        return int(v)
    s, start, count = args
    if kind(s) != "str" or kind(start) != "int" or kind(count) != "int":
        raise RunError("type error")
    if start < 0 or count < 0 or start + count > len(s):
        raise RunError("out of range")
    return s[start:start + count]


class Interpreter:
    def __init__(self, funcs, out):
        self.funcs, self.out = funcs, out
        self.globals = {}
        self.steps = 0
        self.depth = 0

    def step(self, line):
        self.steps += 1
        if self.steps > MAX_STEPS:
            raise RunError("step limit exceeded")

    def lookup(self, scopes, name):
        for s in reversed(scopes):
            if name in s:
                return s
        raise RunError("undefined " + name)

    def eval(self, e, scopes):
        k = e[0]
        if k == "val":
            return e[1]
        if k == "var":
            return self.lookup(scopes, e[1])[e[1]]
        if k == "neg":
            v = self.eval(e[1], scopes)
            if kind(v) != "int":
                raise RunError("type error")
            return checked(-v)
        if k == "not":
            v = self.eval(e[1], scopes)
            if kind(v) != "bool":
                raise RunError("type error")
            return not v
        if k in ("and", "or"):
            left = self.eval(e[1], scopes)
            if kind(left) != "bool":
                raise RunError("type error")
            if left == (k == "or"):
                return left
            right = self.eval(e[2], scopes)
            if kind(right) != "bool":
                raise RunError("type error")
            return right
        if k == "bin":
            a = self.eval(e[2], scopes)
            b = self.eval(e[3], scopes)
            return binary(e[1], a, b)
        return self.call(e[1], e[2], scopes)

    def call(self, name, arg_exprs, scopes):
        if name not in self.funcs and name not in BUILTINS:
            raise RunError("unknown function " + name)
        args = [self.eval(a, scopes) for a in arg_exprs]
        if name in BUILTINS:
            if len(args) != BUILTINS[name]:
                raise RunError("wrong argument count")
            return builtin(name, args)
        f = self.funcs[name]
        if len(args) != len(f["params"]):
            raise RunError("wrong argument count")
        if self.depth >= MAX_DEPTH:
            raise RunError("call depth exceeded")
        self.depth += 1
        try:
            self.block(f["body"], [self.globals, dict(zip(f["params"], args))], fresh=False)
            return 0
        except Return as r:
            return r.value
        finally:
            self.depth -= 1

    def cond(self, e, scopes):
        v = self.eval(e, scopes)
        if kind(v) != "bool":
            raise RunError("type error")
        return v

    def block(self, body, scopes, fresh=True):
        if fresh:
            scopes = scopes + [{}]
        for st in body:
            try:
                self.stmt(st, scopes)
            except RunError as err:
                if err.line is None:
                    err.line = st["line"]
                raise

    def stmt(self, st, scopes):
        k = st["kind"]
        if k == "if":
            for expr, line, body in st["parts"]:
                try:
                    self.step(line)
                    ok = self.cond(expr, scopes)
                except RunError as err:
                    if err.line is None:
                        err.line = line
                    raise
                if ok:
                    self.block(body, scopes)
                    return
            if st["else"] is not None:
                self.block(st["else"], scopes)
            return
        self.step(st["line"])
        if k == "let":
            v = self.eval(st["expr"], scopes)
            if st["name"] in scopes[-1]:
                raise RunError("already declared " + st["name"])
            scopes[-1][st["name"]] = v
        elif k == "set":
            v = self.eval(st["expr"], scopes)
            self.lookup(scopes, st["name"])[st["name"]] = v
        elif k == "print":
            vals = [show(self.eval(e, scopes)) for e in st["exprs"]]
            self.out.append(" ".join(vals))
        elif k == "call":
            self.eval(st["expr"], scopes)
        elif k == "return":
            raise Return(0 if st["expr"] is None else self.eval(st["expr"], scopes))
        elif k == "break":
            raise Break()
        elif k == "continue":
            raise Continue()
        elif k == "while":
            while self.cond(st["expr"], scopes):
                try:
                    self.block(st["body"], scopes)
                except Break:
                    break
                except Continue:
                    pass
                self.step(st["line"])

    def run(self, top):
        self.block(top, [self.globals], fresh=False)
