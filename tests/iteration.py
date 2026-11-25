#!/usr/bin/env python3
"""Spec 046: the two-variable `for` against a model of its meaning, std.lists against Python, and the
cost of a map loop.

  iteration.py                 every group (part of `make test`)
  iteration.py --group NAME    one group: loops, lists, speed

loops  1,000 generated programs (maps, ordered maps, lists, deques; bodies that print, update, add,
       remove, remove and put back, push, pop, break, continue, loop again inside) print what the
       model of specs/046-iteration/contracts/iteration.md says (SC-001)
lists  each function of std.lists against a Python loop on 10,000 random lists (SC-004)
speed  a map of a million entries summed with `for k in keys(m)` + `m[k]` and with `for k, v in m`:
       the new form is not slower (5% for noise) and completes under the smallest memory limit the old
       one completes under (SC-002); reported everywhere, checked on x86-64 outside CI
"""
import json
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
PROGRAMS = 1000 if SLOW == 1 else 100
LISTS = 10000 if SLOW == 1 else 1000

failures = []
cases = 0


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


# ---- loops: a generator of programs and a model of what they print ----

class Stop(Exception):
    def __init__(self, kind, level):
        self.kind, self.level = kind, level


class Gen:
    """One program: a container, a loop over it with two variables, and the container printed after."""

    def __init__(self, rng):
        self.rng = rng
        self.kind = rng.choice(["map", "map", "omap", "list", "list", "deque"])
        self.is_map = self.kind in ("map", "omap")
        n = rng.randint(0, 7)
        if self.is_map:
            keys = rng.sample(range(8), n)
            self.start = [(k, rng.randint(0, 99)) for k in keys]
        else:
            self.start = [rng.randint(0, 99) for _ in range(n)]
        self.named = [rng.random() < 0.85, rng.random() < 0.85]  # level 0: first, second
        self.body = self.block(0, 3)

    def key(self):
        return self.rng.randrange(9)  # 8 is never in the start

    def stmt(self, level, depth):
        r, rng = self.rng.random(), self.rng
        first_named = self.named[0] if level == 0 else True
        if r < 0.25:
            return ("print",)
        if depth > 0 and r < 0.50 and first_named:
            inner = self.block(level, depth - 1)
            if rng.random() < 0.3:
                inner.append((rng.choice(["break", "continue"]),))
            return ("if", self.key() if self.is_map else rng.randrange(8), inner)
        if depth > 1 and level == 0 and r < 0.56:
            return ("nested", self.block(1, 1))
        if self.is_map:
            c = rng.randrange(6)
            if c == 0 and first_named and (self.named[1] if level == 0 else True):
                return ("upd_cur", rng.randint(1, 9))
            if c == 1:
                return ("set", self.key(), rng.randint(100, 199))
            if c == 2:
                return ("rem", self.key())
            if c == 3 and first_named:
                return ("rem_cur",)
            if c == 4:
                return ("readd", self.key(), rng.randint(200, 299))
            return ("print",)
        if self.kind == "list":
            c = rng.randrange(4)
            if c == 0:
                return ("push", rng.randint(100, 199))
            if c == 1:
                return ("pop",)
            if c == 2:
                return ("set_el", rng.randrange(8), rng.randint(200, 299))
            return ("print",)
        return ("push", rng.randint(100, 199)) if rng.random() < 0.5 else ("print",)

    def block(self, level, depth):
        return [self.stmt(level, depth) for _ in range(self.rng.randint(1, 4))]

    # -- the program text --

    def lit(self, k):
        return f'"k{k}"' if self.kind == "map" else str(k)

    def names(self, level):
        if level == 1:
            return "a2", "b2"
        return ("a" if self.named[0] else "_"), ("b" if self.named[1] else "_")

    def emit_block(self, stmts, level, ind, out):
        a, b = self.names(level)
        pad = "    " * ind
        for s in stmts:
            op = s[0]
            if op == "print":
                parts = [f"L{level}"] + [x for x in (a, b) if x != "_"]
                fmtstr = " ".join(["{}"] * len(parts))
                args = ", ".join([f'"L{level}"'] + [x for x in (a, b) if x != "_"])
                out.append(f'{pad}print(format("{fmtstr}", {args}));')
            elif op == "if":
                out.append(f"{pad}if {a} == {self.lit(s[1]) if self.is_map else s[1]} {{")
                self.emit_block(s[2], level, ind + 1, out)
                out.append(f"{pad}}}")
            elif op == "nested":
                out.append(f"{pad}for a2, b2 in c {{")
                self.emit_block(s[1], 1, ind + 1, out)
                out.append(f"{pad}}}")
            elif op == "upd_cur":
                out.append(f"{pad}set c[{a}] = {b} + {s[1]};")
            elif op == "set":
                out.append(f"{pad}set c[{self.lit(s[1])}] = {s[2]};")
            elif op == "rem":
                out.append(f"{pad}remove(c, {self.lit(s[1])});")
            elif op == "rem_cur":
                out.append(f"{pad}remove(c, {a});")
            elif op == "readd":
                out.append(f"{pad}remove(c, {self.lit(s[1])});")
                out.append(f"{pad}set c[{self.lit(s[1])}] = {s[2]};")
            elif op == "push":
                out.append(f"{pad}push(c, {s[1]});")
            elif op == "pop":
                out.append(f"{pad}drop(c);")
            elif op == "set_el":
                out.append(f"{pad}if {s[1]} < len(c) {{")
                out.append(f"{pad}    set c[{s[1]}] = {s[2]};")
                out.append(f"{pad}}}")
            else:
                out.append(f"{pad}{op};")

    def program(self):
        out = []
        if self.kind == "list":
            out += ["fn drop(xs: list[int]) {", "    match pop(xs) {", "        case ok(_) {", "        }",
                    "        case err(_) {", "        }", "    }", "}", ""]
        out.append("fn main() uses console {")
        ty = {"map": "map[str, int]", "omap": "ordered_map[int, int]", "list": "list[int]", "deque": "deque[int]"}[self.kind]
        out.append(f"    let c = new {ty};")
        for item in self.start:
            if self.is_map:
                out.append(f"    set c[{self.lit(item[0])}] = {item[1]};")
            else:
                out.append(f"    push(c, {item});")
        a, b = self.names(0)
        out.append(f"    for {a}, {b} in c {{")
        self.emit_block(self.body, 0, 2, out)
        out.append("    }")
        out.append("    for a, b in c {")
        out.append('        print(format("end {} {}", a, b));')
        out.append("    }")
        out.append("}")
        return "\n".join(out) + "\n"

    # -- the model --

    def show(self, v):
        return f"k{v}" if self.kind == "map" else str(v)

    def visits(self, c):
        """The (first, second-getter) pairs a two-variable loop over c goes through."""
        if self.is_map:
            for k in (sorted(c) if self.kind == "omap" else list(c)):  # the keys at the start
                if k in c:
                    yield k
        elif self.kind == "list":
            for i in range(len(c)):  # the indexes at the start
                if i >= len(c):
                    return
                yield i
        else:
            yield from range(len(c))

    def run_block(self, stmts, level, env, c, snap, out):
        a, b = env[level]
        an, bn = (True, True) if level == 1 else self.named
        for s in stmts:
            op = s[0]
            if op == "print":
                parts = [f"L{level}"]
                if an:
                    parts.append(self.show(a) if self.is_map else str(a))
                if bn:
                    parts.append(str(b))
                out.append(" ".join(parts))
            elif op == "if":
                if a == s[1]:
                    self.run_block(s[2], level, env, c, snap, out)
            elif op == "nested":
                self.loop(s[1], 1, env, c, out)
            elif op == "upd_cur":
                c[a] = b + s[1]
            elif op == "set":
                c[s[1]] = s[2]
            elif op in ("rem", "rem_cur"):
                c.pop(s[1] if op == "rem" else a, None)
            elif op == "readd":
                c.pop(s[1], None)
                c[s[1]] = s[2]
            elif op == "push":
                c.append(s[1])
            elif op == "pop":
                if c:
                    c.pop()
            elif op == "set_el":
                if s[1] < len(c):
                    c[s[1]] = s[2]
            else:
                raise Stop(op, level)

    def loop(self, body, level, env, c, out):
        snap = list(c) if self.kind == "deque" else None  # a deque: its elements at the start
        for first in self.visits(c):
            second = snap[first] if snap is not None else c[first]
            env[level] = (first, second)
            try:
                self.run_block(body, level, env, c, snap, out)
            except Stop as s:
                if s.kind == "break":
                    break

    def expected(self):
        c = dict(self.start) if self.is_map else list(self.start)
        out = []
        self.loop(self.body, 0, {}, c, out)
        if self.is_map:
            out += [f"end {self.show(k)} {c[k]}" for k in (sorted(c) if self.kind == "omap" else c)]
        else:
            out += [f"end {i} {x}" for i, x in enumerate(c)]
        return "".join(line + "\n" for line in out)


def group_loops(d):
    rng = random.Random(46)
    wrong, kinds = [], {}
    for n in range(PROGRAMS):
        g = Gen(rng)
        kinds[g.kind] = kinds.get(g.kind, 0) + 1
        path = os.path.join(d, "p.vo")
        with open(path, "w") as f:
            f.write(g.program())
        r = subprocess.run([VO, "run", "--timeout", str(20 * SLOW), "p.vo"], cwd=d, capture_output=True, text=True, timeout=120 * SLOW)
        out = "".join(line for line in r.stderr.splitlines(True) if "warning" in line and "W0201" not in line)
        if r.returncode != 0 or r.stdout != g.expected() or out:
            wrong.append(n)
            if len(wrong) <= 2:
                keep = os.path.join(ROOT, "build", "tmp", f"iteration-fail-{n}.vo")
                with open(keep, "w") as f:
                    f.write(g.program())
                print(f"program {n} ({g.kind}) kept as {keep}: exit {r.returncode}\n--- expected\n{g.expected()}--- got\n{r.stdout}{r.stderr[:600]}")
    print(f"iteration: {PROGRAMS} programs: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())))
    case(f"loops/{PROGRAMS} generated programs print what the model says (SC-001)", not wrong, f"{len(wrong)} differ: {wrong[:10]}")


# ---- lists ----

LISTS_DRIVER = """import std.fs;
import std.json;
import std.lists;

fn text(xs: list[str]) -> result[str] {
    return json.write_json(json.to_json(xs));
}

fn first_or(r: result[str]) -> str {
    match r {
        case ok(x) {
            return "ok " + x;
        }
        case err(e) {
            return "err " + e;
        }
    }
}

fn size(w: str) -> int {
    return len(w);
}

fn itself(w: str) -> str {
    return w;
}

fn half(w: str) -> float {
    return float(len(w)) / 2.0;
}

fn answer(f: list[str]) -> result[str] {
    if f[0] == "sum" {
        let ns: list[int] = try json.from_json(try json.parse_json(f[1]));
        return ok(str(lists.sum(ns)));
    }
    let xs: list[str] = try json.from_json(try json.parse_json(f[1]));
    let before = try text(xs);
    var out = "";
    if f[0] == "contains" {
        let x: str = try json.from_json(try json.parse_json(f[2]));
        set out = format("{} {}", lists.has_item(xs, x), lists.index_of(xs, x));
    } else if f[0] == "reversed" {
        set out = try text(lists.reversed(xs));
    } else if f[0] == "desc" {
        let ys = lists.reversed(lists.reversed(xs));
        if f[2] == "size" {
            lists.sort_by_desc(ys, size);
        } else if f[2] == "itself" {
            lists.sort_by_desc(ys, itself);
        } else {
            lists.sort_by_desc(ys, half);
        }
        set out = try text(ys);
    } else if f[0] == "minmax" {
        set out = first_or(lists.min_by(xs, size)) + " " + first_or(lists.max_by(xs, itself));
    } else {
        let groups = lists.group_by(xs, size);
        for k, g in groups {
            set out = out + str(k) + ":" + try text(g) + ";";
        }
    }
    if try text(xs) != before {
        return ok("the argument changed");
    }
    return ok(out);
}

fn main() -> result[void] uses console, file.read("./") {
    try fs.each_line("cases.txt", fn(line: str) -> result[bool] {
        print(try answer(split(line, "\\t")));
        return ok(true);
    });
    return ok();
}
"""


def js(v):
    return json.dumps(v, ensure_ascii=False, separators=(",", ":"))


def group_lists(d):
    rng = random.Random(146)
    words = ["", "a", "b", "ab", "ba", "abc", "é", "zz", "abcd", "k", "ö", "日本", "aa", "B", "A"]
    lines, want = [], []
    per = LISTS // 5
    for i in range(LISTS):
        xs = [rng.choice(words) for _ in range(rng.choice([0, 0, 1, 2, 3, 5, 8, 13, 40]))]
        kind = i // per if i < 5 * per else 4
        if kind == 0:
            x = rng.choice(words)
            lines.append(f"contains\t{js(xs)}\t{js(x)}")
            want.append(f"{'true' if x in xs else 'false'} {xs.index(x) if x in xs else -1}")
        elif kind == 1:
            lines.append(f"reversed\t{js(xs)}")
            want.append(js(xs[::-1]))
        elif kind == 2:
            key = rng.choice(["size", "itself", "half"])
            lines.append(f"desc\t{js(xs)}\t{key}")
            f = (lambda w: w.encode()) if key == "itself" else (lambda w: len(w.encode()))
            want.append(js(sorted(xs, key=f, reverse=True)))  # Python's reverse sort is stable too
        elif kind == 3:
            lines.append(f"minmax\t{js(xs)}")
            if xs:
                lo = min(xs, key=lambda w: len(w.encode()))  # the first of the smallest
                hi = max(xs, key=lambda w: w.encode())       # the first of the largest
                want.append(f"ok {lo} ok {hi}")
            else:
                want.append("err empty list err empty list")
        else:
            lines.append(f"group\t{js(xs)}")
            groups = {}
            for w in xs:
                groups.setdefault(len(w.encode()), []).append(w)
            want.append("".join(f"{k}:{js(g)};" for k, g in groups.items()))
    for _ in range(200):
        ns = [rng.randint(-10**6, 10**6) for _ in range(rng.randint(0, 30))]
        lines.append(f"sum\t{js(ns)}")
        want.append(str(sum(ns)))
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(LISTS_DRIVER)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("".join(line + "\n" for line in lines))
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW), "main.vo"], cwd=d, capture_output=True, text=True, timeout=600 * SLOW)
    out = r.stdout.split("\n")[:-1]
    if r.returncode != 0 or len(out) != len(lines):
        case("lists/the driver runs", False, f"exit {r.returncode}, {len(out)} lines for {len(lines)}: {r.stderr[:500]}")
        return
    wrong = [f"{lines[i]!r}: {out[i]!r} want {want[i]!r}" for i in range(len(lines)) if out[i] != want[i]]
    case(f"lists/{len(lines)} calls agree with Python (SC-004)", not wrong, f"{len(wrong)}: {wrong[:3]}")


# ---- speed ----

SPEED = """fn build() -> map[int, int] {
    let m = new map[int, int];
    for i in 0..1000000 {
        set m[i * 7] = i;
    }
    return m;
}

fn old(m: map[int, int]) -> int {
    var total = 0;
    for k in keys(m) {
        let v = m[k];
        set total = total + k + v;
    }
    return total;
}

fn new_form(m: map[int, int]) -> int {
    var total = 0;
    for k, v in m {
        set total = total + k + v;
    }
    return total;
}

fn main() uses console, clock, env {
    let m = build();
    let which = env("FORM");
    var best = 1000000;
    var total = 0;
    for _ in 0..5 {
        let t0 = now_ms();
        if which == ok("old") {
            set total = old(m);
        } else {
            set total = new_form(m);
        }
        set best = min(best, now_ms() - t0);
    }
    print(format("{} {}", total, best));
}
"""


def group_speed(d):
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(SPEED)

    def run(form, limit=None):
        cmd = [VO, "run", "--timeout", str(300 * SLOW)] + (["--max-memory", str(limit)] if limit else []) + ["main.vo"]
        return subprocess.run(cmd, cwd=d, capture_output=True, text=True, timeout=600 * SLOW, env={**os.environ, "FORM": form})

    times = {}
    for form in ("old", "new"):
        r = run(form)
        parts = r.stdout.split()
        if r.returncode != 0 or len(parts) != 2 or parts[0] != str(sum(i * 8 for i in range(1000000))):
            case(f"speed/the {form} form runs and sums right", False, f"exit {r.returncode} {r.stdout[:80]!r} {r.stderr[:400]}")
            return
        times[form] = int(parts[1])
    if SLOW != 1:
        print(f"iteration: a million entries: keys form {times['old']} ms, two-variable form {times['new']} ms (emulated; memory not searched)")
        return
    lo, hi = 1, 64  # in steps of 8 MiB: the smallest limit the old form completes under
    while lo < hi:
        mid = (lo + hi) // 2
        if run("old", mid * 8).returncode == 0:
            hi = mid
        else:
            lo = mid + 1
    limit = lo * 8
    fits = run("new", limit).returncode == 0
    print(f"iteration: a million entries: keys form {times['old']} ms, two-variable form {times['new']} ms; "
          f"the keys form needs --max-memory {limit}, the two-variable form {'completes' if fits else 'does not complete'} under it")
    case("speed/the two-variable form completes under the keys form's smallest limit (SC-002)", fits, f"{limit} MiB")
    if not os.environ.get("CI"):
        case("speed/the two-variable form is not slower (SC-002)", times["new"] <= times["old"] * 1.05, f"{times}")


GROUPS = {"loops": group_loops, "lists": group_lists, "speed": group_speed}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    tmp = os.path.join(ROOT, "build", "tmp")
    os.makedirs(tmp, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=tmp) as base:
        for n in names:
            sub = os.path.join(base, n)
            os.makedirs(sub)
            GROUPS[n](sub)
    print(f"iteration: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
