#!/usr/bin/env python3
"""Seeded generator of type-correct vo programs (spec 004, research R13, FR-015).

  gen_programs.py --seed S     print one program

generate(seed) -> str is used by tests/diff_engines.py. Programs print as they go, so any difference
in execution shows up in stdout. About one in ten stops on a runtime error on purpose (division by a
value that may be zero, an index that may be out of range, a missing key, overflow). Loop bounds and
recursion depth are capped so that every program runs well under 1 s on the tree walker.
"""
import random
import sys

PRELUDE = """record Pt {
    x: int;
    y: int;
}

variant Shape {
    none;
    one(int);
    two(int);
}

record Box[T] {
    value: T;
}

fn shape_of(n: int) -> Shape {
    if n % 3 == 0 {
        return Shape.none;
    }
    if n % 3 == 1 {
        return Shape.one(n);
    }
    return Shape.two(n + 1);
}

fn shape_val(s: Shape) -> int {
    match s {
        case none {
            return 0;
        }
        case one(a) {
            return a;
        }
        case two(a) {
            return a * 2;
        }
    }
}

fn pick[T: ordered](a: T, b: T) -> T {
    if a > b {
        return a;
    }
    return b;
}

fn tri(n: int) -> int {
    if n <= 0 {
        return 0;
    }
    return n + tri(n - 1);
}

fn checked_div(a: int, b: int) -> result[int] {
    if b == 0 {
        return err("div by zero");
    }
    return ok(a / b);
}

fn chain(a: int, b: int, c: int) -> result[int] {
    let q = try checked_div(a, b);
    let r = try checked_div(q, c);
    return ok(q + r);
}

fn noisy(b: bool, tag: int) -> bool uses console {
    print("noisy " + str(tag));
    return b;
}

fn side(v: int, tag: str) -> int uses console {
    print("side " + tag + " " + str(v));
    return v;
}
"""



class Gen:
    def __init__(self, seed):
        self.r = random.Random(seed)
        self.n = 0
        # scopes: list of dicts name -> (type, mutable)
        self.scopes = [{}]
        self.loop_depth = 0
        self.depth = 0
        self.lines = []
        self.ind = 1
        self.helpers = []
        self.in_helper = False
        self.budget = 0

    # ---- names and scopes ----
    def fresh(self, prefix):
        self.n += 1
        return f"{prefix}{self.n}"

    def vars_of(self, ty, mutable=None):
        out = []
        for sc in self.scopes:
            for name, (t, m) in sc.items():
                if t == ty and (mutable is None or m == mutable):
                    out.append(name)
        return out

    def declare(self, name, ty, mutable):
        self.scopes[-1][name] = (ty, mutable)

    def emit(self, s):
        self.lines.append("    " * self.ind + s)

    # ---- expressions ----
    def int_expr(self, d=0):
        r = self.r
        vs = self.vars_of("int")
        choices = ["lit", "lit", "var", "var", "bin", "bin", "len", "call"]
        if d >= 3:
            choices = ["lit", "var", "var"]
        c = r.choice(choices)
        if c == "var" and vs:
            return r.choice(vs)
        if c == "bin":
            op = r.choice(["+", "-", "*", "%", "/", "+", "*"])
            a, b = self.int_expr(d + 1), self.int_expr(d + 1)
            if op in "/%":
                if r.random() < 0.97:
                    return f"({a} {op} (abs({b}) + 1))"
                return f"({a} {op} {b})"  # may divide by zero: R0002 on purpose
            if op == "*":
                if r.random() < 0.02:
                    return f"(9223372036854775000 + {a} * {b})"  # overflows when a * b > 807: R0001 on purpose
                return f"(({a} * {b}) % 10007)"
            return f"({a} {op} {b})"
        if c == "len":
            ls = self.vars_of("list[int]") + self.vars_of("list[str]") + self.vars_of("str")
            if ls:
                return f"len({r.choice(ls)})"
        if c == "call":
            k = r.randrange(10)
            a = self.int_expr(d + 1)
            if k == 0:
                return f"tri(abs({a}) % 40)"
            if k == 1:
                return f"pick({a}, {self.int_expr(d + 1)})"
            if k == 2:
                return f"shape_val(shape_of({a}))"
            if k == 3:
                pts = self.vars_of("Pt")
                if pts:
                    return f"{r.choice(pts)}.{r.choice('xy')}"
            if k == 4:
                ls = self.vars_of("list[int]")
                if ls:
                    xs = r.choice(ls)
                    if r.random() < 0.95:
                        return f"fold({xs}, {a}, fn(acc: int, v: int) -> int {{\n{self._lam_body('(acc + v) % 100003')}}})"
                    return f"{xs}[{a}]"  # may be out of range: R0003 on purpose
            if k == 5:
                ls = self.vars_of("list[int]")
                if ls:
                    xs = r.choice(ls)
                    return f"{xs}[abs({a}) % (len({xs}) + 1) % pick(len({xs}), 1)]"
            if k == 6:
                ms = self.vars_of("map[str, int]")
                if ms:
                    m = r.choice(ms)
                    key = self.str_key()
                    if r.random() < 0.9:
                        return f"get_or({m}, {key})"
                    return f"{m}[{key}]"  # may be missing: R0004 on purpose
            if k == 7 and not self.in_helper:
                return f"side({a}, \"{self.fresh('t')}\")"
            if k == 8 and self.helpers:
                h = r.choice(self.helpers)
                return f"{h}({a}, {self.int_expr(d + 1)})"
            if k == 9:
                bs = self.vars_of("Box[int]")
                if bs:
                    return f"{r.choice(bs)}.value"
        return str(r.choice([0, 1, 2, 3, 5, 7, 10, 42, 99, 1000, r.randrange(-50, 50)]))

    def _lam_body(self, ret):
        return "    " * (self.ind + 1) + f"return {ret};\n" + "    " * self.ind

    def str_key(self):
        return f"\"k{self.r.randrange(6)}\""

    def str_expr(self, d=0):
        r = self.r
        vs = self.vars_of("str")
        c = r.choice(["lit", "var", "var", "cat", "str", "join", "up"] if d < 3 else ["lit", "var"])
        if c == "var" and vs:
            return r.choice(vs)
        if c == "cat":
            return f"{self.str_expr(d + 1)} + {self.str_expr(d + 1)}"
        if c == "str":
            return f"str({self.int_expr(d + 1)})"
        if c == "join":
            ls = self.vars_of("list[str]")
            if ls:
                return f"join({r.choice(ls)}, \",\")"
        if c == "up":
            return f"{r.choice(['upper', 'lower', 'trim'])}({self.str_expr(d + 1)})"
        return "\"" + r.choice(["a", "bc", "Hello", " x ", "k1", "k2", "", "zz"]) + "\""

    def bool_expr(self, d=0):
        r = self.r
        c = r.choice(["cmp", "cmp", "and", "eq", "has", "lit"] if d < 2 else ["cmp", "lit"])
        if c == "cmp":
            return f"{self.int_expr(d + 1)} {r.choice(['<', '<=', '>', '>=', '==', '!='])} {self.int_expr(d + 1)}"
        if c == "and":
            a, b = self.bool_expr(d + 1), self.bool_expr(d + 1)
            if not self.in_helper and r.random() < 0.5:
                b = f"noisy({b}, {self.r.randrange(100)})"
            return f"({a} {r.choice(['and', 'or'])} {b})"
        if c == "eq":
            ls = self.vars_of("list[int]")
            if len(ls) >= 1 and r.random() < 0.5:
                return f"{r.choice(ls)} == {r.choice(ls)}"
            return f"{self.str_expr(d + 1)} {r.choice(['==', '<', '!='])} {self.str_expr(d + 1)}"
        if c == "has":
            ms = self.vars_of("map[str, int]")
            if ms:
                return f"has({r.choice(ms)}, {self.str_key()})"
        return r.choice(["true", "false"])

    def list_int_expr(self):
        r = self.r
        ls = self.vars_of("list[int]")
        c = r.choice(["lit", "map", "filter", "var", "slice"])
        if c == "map" and ls:
            caps = self.vars_of("int", mutable=False)
            extra = f" + {r.choice(caps)}" if caps else ""
            return f"map({r.choice(ls)}, fn(v: int) -> int {{\n{self._lam_body(f'(v * 3{extra}) % 1009')}}})"
        if c == "filter" and ls:
            return f"filter({r.choice(ls)}, fn(v: int) -> bool {{\n{self._lam_body('v % 2 == 0')}}})"
        if c == "var" and ls:
            return f"slice({r.choice(ls)}, 0, len({r.choice(ls)}) / 2)" if False else r.choice(ls)
        if c == "slice" and ls:
            xs = r.choice(ls)
            return f"slice({xs}, 0, len({xs}) / 2)"
        n = r.randrange(0, 5)
        if n == 0:
            return "new list[int]"
        return "[" + ", ".join(self.int_expr(2) for _ in range(n)) + "]"

    # ---- statements ----
    def stmt(self):
        r = self.r
        self.budget -= 1
        kinds = ["let_int", "var_int", "set_int", "set_int", "print", "print", "if", "list", "push",
                 "set_idx", "map", "pt", "match", "for", "while", "try", "closures", "sort", "str", "box",
                 "float"]
        if self.depth >= 2 or self.budget <= 0:
            kinds = ["let_int", "set_int", "print", "push", "set_idx"]
        if self.loop_depth:
            kinds += ["brk"]
        k = r.choice(kinds)
        getattr(self, "s_" + k)()

    def block(self, n):
        self.scopes.append({})
        self.ind += 1
        self.depth += 1
        for _ in range(n):
            self.stmt()
        self.depth -= 1
        self.ind -= 1
        self.scopes.pop()

    def s_let_int(self):
        n = self.fresh("a")
        self.emit(f"let {n} = {self.int_expr()};")
        self.declare(n, "int", False)

    def s_var_int(self):
        n = self.fresh("v")
        self.emit(f"var {n} = {self.int_expr()};")
        self.declare(n, "int", True)

    def s_set_int(self):
        vs = self.vars_of("int", mutable=True)
        if not vs:
            return self.s_var_int()
        self.emit(f"set {self.r.choice(vs)} = {self.int_expr()};")

    def s_print(self):
        if self.in_helper:
            return self.s_set_int()
        c = self.r.randrange(4)
        if c == 0:
            self.emit(f"print(str({self.int_expr()}));")
        elif c == 1:
            self.emit(f"print({self.str_expr()});")
        elif c == 2:
            self.emit(f"print(str({self.bool_expr()}));")
        else:
            ls = self.vars_of("list[int]")
            if ls:
                xs = self.r.choice(ls)
                self.emit(f"print(join(map({xs}, fn(v: int) -> str {{\n{self._lam_body('str(v)')}}}), \" \"));")
            else:
                self.emit(f"print(str({self.int_expr()}));")

    def s_if(self):
        self.emit(f"if {self.bool_expr()} {{")
        self.block(self.r.randrange(1, 4))
        if self.r.random() < 0.3:
            self.emit(f"}} else if {self.bool_expr()} {{")
            self.block(self.r.randrange(1, 3))
        if self.r.random() < 0.5:
            self.emit("} else {")
            self.block(self.r.randrange(1, 3))
        self.emit("}")

    def s_list(self):
        n = self.fresh("xs")
        self.emit(f"let {n} = {self.list_int_expr()};")
        self.declare(n, "list[int]", False)

    def s_push(self):
        ls = self.vars_of("list[int]")
        if not ls:
            return self.s_list()
        xs = self.r.choice(ls)
        self.emit(f"if len({xs}) < 40 {{")
        self.emit(f"    push({xs}, {self.int_expr()});")
        self.emit("}")

    def s_set_idx(self):
        ls = self.vars_of("list[int]")
        if not ls:
            return self.s_list()
        xs = self.r.choice(ls)
        if self.r.random() < 0.97:
            self.emit(f"if len({xs}) > 0 {{")
            self.emit(f"    set {xs}[abs({self.int_expr(1)}) % len({xs})] = {self.int_expr(1)};")
            self.emit("}")
        else:
            # out of range on purpose (R0003), with a side effect evaluated before the check
            self.emit(f"set {xs}[{self.int_expr(1)}] = {'side(1, \"idx\")' if not self.in_helper else '1'};")

    def s_map(self):
        ms = self.vars_of("map[str, int]")
        if not ms or self.r.random() < 0.3:
            n = self.fresh("m")
            self.emit(f"let {n} = new map[str, int];")
            self.declare(n, "map[str, int]", False)
            ms = [n]
        m = self.r.choice(ms)
        c = self.r.randrange(3)
        if c == 0:
            self.emit(f"set {m}[{self.str_key()}] = {self.int_expr()};")
        elif c == 1:
            self.emit(f"remove({m}, {self.str_key()});")
        else:
            if not self.in_helper:
                self.emit(f"print(join(keys({m}), \"|\"));")

    def s_pt(self):
        pts = self.vars_of("Pt")
        if pts and self.r.random() < 0.5:
            self.emit(f"set {self.r.choice(pts)}.{self.r.choice('xy')} = {self.int_expr()};")
            return
        n = self.fresh("p")
        self.emit(f"let {n} = new Pt {{ x: {self.int_expr(1)}, y: {self.int_expr(1)} }};")
        self.declare(n, "Pt", False)

    def s_box(self):
        n = self.fresh("b")
        self.emit(f"let {n} = new Box[int] {{ value: {self.int_expr(1)} }};")
        self.declare(n, "Box[int]", False)

    def s_match(self):
        a = self.fresh("c")
        b = self.fresh("c")
        self.emit(f"match shape_of({self.int_expr()}) {{")
        self.ind += 1
        arms = [("none", None), ("one", a), ("two", b)]
        self.r.shuffle(arms)
        for case, bind in arms:
            self.emit(f"case {case}{'(' + bind + ')' if bind else ''} {{")
            self.scopes.append({})
            if bind:
                self.declare(bind, "int", False)
            self.ind += 1
            self.depth += 1
            for _ in range(self.r.randrange(1, 3)):
                self.stmt()
            self.depth -= 1
            self.ind -= 1
            self.scopes.pop()
            self.emit("}")
        self.ind -= 1
        self.emit("}")

    def s_for(self):
        i = self.fresh("i")
        if self.r.random() < 0.5 or not self.vars_of("list[int]"):
            self.emit(f"for {i} in {self.r.randrange(0, 3)}..{self.r.randrange(2, 9)} {{")
        else:
            xs = self.r.choice(self.vars_of("list[int]"))
            self.emit(f"for {i} in {xs} {{")
            if self.r.random() < 0.3:  # a list that grows inside its own loop
                self.emit(f"    if len({xs}) < 12 {{")
                self.emit(f"        push({xs}, {i} + 1);")
                self.emit("    }")
        self.scopes.append({i: ("int", False)})
        self.loop_depth += 1
        self.block(self.r.randrange(1, 4))
        self.loop_depth -= 1
        self.scopes.pop()
        self.emit("}")

    def s_while(self):
        w = self.fresh("w")
        self.emit(f"var {w} = 0;")
        self.emit(f"while {w} < {self.r.randrange(1, 7)} {{")
        self.emit(f"    set {w} = {w} + 1;")
        self.loop_depth += 1
        self.block(self.r.randrange(1, 4))
        self.loop_depth -= 1
        self.emit("}")
        self.declare(w, "int", True)

    def s_brk(self):
        self.emit(f"if {self.bool_expr()} {{")
        self.emit(f"    {self.r.choice(['break', 'continue'])};")
        self.emit("}")

    def s_try(self):
        if self.in_helper:
            return self.s_set_int()
        r = self.fresh("r")
        self.emit(f"match chain({self.int_expr()}, {self.int_expr()} % 3, {self.int_expr()} % 4) {{")
        self.emit(f"    case ok({r}) {{")
        self.emit(f"        print(\"ok \" + str({r}));")
        self.emit("    }")
        self.emit("    case err(e) {")
        self.emit("        print(\"err \" + e);")
        self.emit("    }")
        self.emit("}")

    def s_closures(self):
        fs = self.fresh("fs")
        i = self.fresh("i")
        k = self.fresh("k")
        self.emit(f"let {fs} = new list[fn(int) -> int];")
        self.emit(f"for {i} in 0..{self.r.randrange(1, 5)} {{")
        self.emit(f"    let {k} = {i} * {self.r.randrange(1, 9)};")
        self.emit(f"    push({fs}, fn(v: int) -> int {{")
        self.emit(f"        return v + {k};")
        self.emit("    });")
        self.emit("}")
        if not self.in_helper:
            self.emit(f"for {self.fresh('g')} in {fs} {{")
            self.emit(f"    print(str(g{self.n}({self.r.randrange(10)})));")
            self.emit("}")

    def s_sort(self):
        ls = self.vars_of("list[int]")
        if not ls:
            return self.s_list()
        xs = self.r.choice(ls)
        if self.r.random() < 0.5:
            self.emit(f"sort({xs});")
        else:
            self.emit(f"sort_by({xs}, fn(v: int) -> int {{")
            self.emit(f"    return {self.r.choice(['0 - v', 'v % 5', 'v'])};")
            self.emit("});")

    def s_str(self):
        n = self.fresh("s")
        self.emit(f"let {n} = {self.str_expr()};")
        self.declare(n, "str", False)
        if self.r.random() < 0.5:
            ls = self.fresh("ss")
            self.emit(f"let {ls} = split({n} + \",x,\" + {self.str_expr(1)}, \",\");")
            self.declare(ls, "list[str]", False)

    def s_float(self):
        if self.in_helper:
            return self.s_let_int()
        f = self.fresh("f")
        self.emit(f"let {f} = float({self.int_expr(1)}) / {self.r.choice(['3.0', '7.5', '2.0'])};")
        self.emit(f"print(str({f} * 1.5) + \" \" + str(int({f})));")

    # ---- program ----
    def helper(self, name):
        self.in_helper = True
        self.lines.append(f"fn {name}(x: int, y: int) -> int {{")
        self.scopes = [{"x": ("int", False), "y": ("int", False)}]
        self.ind = 1
        self.budget = 8
        for _ in range(self.r.randrange(1, 5)):
            self.stmt()
        self.emit(f"return {self.int_expr(1)};")
        self.lines.append("}")
        self.lines.append("")
        self.in_helper = False
        self.helpers.append(name)

    def program(self):
        for h in range(self.r.randrange(0, 3)):
            self.helper(f"h{h}")
        self.lines.append("fn main() uses console {")
        self.scopes = [{}]
        self.ind = 1
        self.budget = 40
        for _ in range(self.r.randrange(6, 16)):
            self.stmt()
        self.lines.append("}")
        return PRELUDE + "\nfn get_or(m: map[str, int], k: str) -> int {\n    if has(m, k) {\n        return m[k];\n    }\n    return -1;\n}\n\n" + "\n".join(self.lines) + "\n"


def generate(seed):
    return Gen(seed).program()


if __name__ == "__main__":
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else 1
    sys.stdout.write(generate(seed))
