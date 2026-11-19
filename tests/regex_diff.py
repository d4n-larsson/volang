#!/usr/bin/env python3
"""Spec 025 SC-001: std.regex against Python's re (research R8).

Generates random patterns over the supported syntax and random texts, runs `all_matches`, `split_on`
and `replace_all` in vo programs of 2,000 pairs, and compares with Python's re.finditer (ASCII
classes; `$` outside multiline written `\\Z` for Python; `(?<name>` written `(?P<name>`). Offsets are
compared in bytes. Excluded: `\\B` on an empty text (Python before 3.14 never matches it there).

  regex_diff.py [--quick] [--pairs N] [--seed S] [--cross BIN...]

--quick runs 5,000 pairs (default 100,000). --cross runs the same programs with each build (through
bin/vo, VO_BIN=BIN) and requires identical output. Exit 1 on any difference.
"""
import os
import random
import re
import signal
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CHUNK = 2000
LITS = ["a", "b", "c", "-", " ", "1", "2", "A"]
TEXT = "abc- 12A\né"


class Gen:
    def __init__(self, rng):
        self.rng = rng
        self.names = 0

    def atom(self, depth, multi):
        """(vo text, python text, nullable, has_capture)"""
        r = self.rng.random()
        if r < 0.40:
            c = self.rng.choice(LITS)
            return c, c, False, False
        if r < 0.47:
            return ".", ".", False, False
        if r < 0.57:
            sh = self.rng.choice(["\\d", "\\w", "\\s", "\\D", "\\W", "\\S"])
            return sh, sh, False, False
        if r < 0.67:
            items = self.rng.sample(["a", "b", "c", "1", "\\d", "\\s", " ", "a-c", "0-9", "A-Z", "é"], self.rng.randint(1, 3))
            neg = "^" if self.rng.random() < 0.3 else ""
            t = "[" + neg + "".join(items) + "]"
            return t, t, False, False
        if r < 0.75:
            a = self.rng.choice(["^", "$", "\\b", "\\B"])
            py = "\\Z" if a == "$" and not multi else a
            return a, py, True, False
        if depth > 2:
            c = self.rng.choice(LITS)
            return c, c, False, False
        kind = self.rng.random()
        v, p, nul, cap = self.alt(depth + 1, multi)
        if kind < 0.5:
            return "(" + v + ")", "(" + p + ")", nul, True
        if kind < 0.7:
            self.names += 1
            n = f"g{self.names}"
            return f"(?<{n}>{v})", f"(?P<{n}>{p})", nul, True
        if kind < 0.85:
            return "(?:" + v + ")", "(?:" + p + ")", nul, cap
        fl = self.rng.choice(["i", "s", "m"])
        v2, p2, nul2, cap2 = self.alt(depth + 1, multi or fl == "m")
        return f"(?{fl}:{v2})", f"(?{fl}:{p2})", nul2, cap2

    def piece(self, depth, multi):
        v, p, nul, cap = self.atom(depth, multi)
        if v in ("^", "$", "\\b", "\\B"):
            return v, p, nul, cap
        q = self.rng.random()
        if q < 0.6:
            return v, p, nul, cap
        quant = self.rng.choice(["*", "+", "?", "{2}", "{1,2}", "{0,2}", "{2,}"])
        if self.rng.random() < 0.3:
            quant += "?"
        mn_zero = quant[0] in "*?" or quant.startswith("{0")
        return v + quant, p + quant, nul or mn_zero, cap

    def cat(self, depth, multi):
        parts = [self.piece(depth, multi) for _ in range(self.rng.randint(1, 3))]
        return ("".join(x[0] for x in parts), "".join(x[1] for x in parts),
                all(x[2] for x in parts), any(x[3] for x in parts))

    def alt(self, depth, multi):
        branches = [self.cat(depth, multi) for _ in range(1 if self.rng.random() < 0.7 else 2)]
        return ("|".join(b[0] for b in branches), "|".join(b[1] for b in branches),
                any(b[2] for b in branches), any(b[3] for b in branches))

    def pattern(self):
        self.names = 0
        flag = self.rng.choice(["", "", "", "(?i)", "(?m)", "(?s)"])
        v, p, _, _ = self.alt(0, flag == "(?m)")
        return flag + v, flag + p

    def text(self):
        return "".join(self.rng.choice(TEXT) for _ in range(self.rng.randint(0, 12)))


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t") + '"'


class Slow(Exception):
    pass


def _alarm(*_):
    raise Slow()


def expected(py, s):
    """The lines show() prints for one pair; Slow when Python's backtracking takes over 0.2 s (the
    patterns vo exists to make safe: such pairs are skipped and counted)."""
    signal.signal(signal.SIGALRM, _alarm)
    signal.setitimer(signal.ITIMER_REAL, 0.2)
    try:
        return _expected(py, s)
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)


def _expected(py, s):
    rx = re.compile(py, re.ASCII)
    b = lambda i: len(s[:i].encode())
    ms = list(rx.finditer(s))
    one = []
    for m in ms:
        gs = [m.group(i) if m.group(i) is not None else "~" for i in range(rx.groups + 1)]
        one.append(f"{b(m.start())}:{b(m.end())}:" + ",".join(gs))
    parts, last = [], 0
    for m in ms:
        parts.append(s[last:m.start()])
        last = m.end()
    parts.append(s[last:])
    repl = rx.sub(lambda m: "<" + m.group(0) + ">", s)
    return ";".join(one) + "\n" + "|".join(parts) + "\n" + repl


PRELUDE = """import std.regex.regex;
import std.regex.Regex;
import std.regex.all_matches;
import std.regex.split_on;
import std.regex.replace_all;

fn show(k: int, re: Regex, s: str) uses console {
    print("#@#" + str(k));
    var one = new list[str];
    for m in all_matches(re, s) {
        var gs = new list[str];
        for i in 0..len(m.groups) {
            if m.took_part[i] {
                push(gs, m.groups[i]);
            } else {
                push(gs, "~");
            }
        }
        push(one, str(m.start) + ":" + str(m.end) + ":" + join(gs, ","));
    }
    print(join(one, ";"));
    print(join(split_on(re, s), "|"));
    print(replace_all(re, s, "<$0>"));
}

fn main() uses console {
"""


def run(env, path):
    p = subprocess.run([VO, "run", path], capture_output=True, env=env, timeout=600)
    if p.returncode != 0:
        sys.exit(f"regex_diff: {path} exited {p.returncode}: {p.stderr.decode(errors='replace')[:3000]}")
    return p.stdout.decode()


def chunks(out):
    res = {}
    for block in out.split("#@#")[1:]:
        k, _, rest = block.partition("\n")
        res[int(k)] = rest[:-1] if rest.endswith("\n") else rest
    return res


def main():
    args = sys.argv[1:]
    n = 5000 if "--quick" in args else 100000
    if "--pairs" in args:
        n = int(args[args.index("--pairs") + 1])
    seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 1
    cross = args[args.index("--cross") + 1:] if "--cross" in args else []
    rng = random.Random(seed)
    g = Gen(rng)
    pairs = []
    slow = 0
    while len(pairs) < n:
        v, p = g.pattern()
        try:
            re.compile(p, re.ASCII)
        except re.error:
            continue
        s = g.text()
        if "\\B" in v and s == "":  # Python before 3.14: \B never matches an empty text (a quirk)
            continue
        try:
            want = expected(p, s)
        except Slow:
            slow += 1
            continue
        pairs.append((v, p, s, want))
    tmp = tempfile.mkdtemp(prefix="voregex-")
    fails = 0
    try:
        for c0 in range(0, len(pairs), CHUNK):
            part = pairs[c0:c0 + CHUNK]
            path = os.path.join(tmp, f"p{c0 // CHUNK}.vo")
            with open(path, "w") as f:
                f.write(PRELUDE)
                for i, (v, _, s, _) in enumerate(part):
                    f.write(f"    show({c0 + i}, regex({vo_str(v)}), {vo_str(s)});\n")
                f.write("}\n")
            out = run(dict(os.environ), path)
            got = chunks(out)
            for i, (v, p, s, want) in enumerate(part):
                if got.get(c0 + i) != want:
                    fails += 1
                    if fails <= 5:
                        print(f"FAIL pattern {v!r} text {s!r}\n  vo:     {got.get(c0 + i)!r}\n  python: {want!r}")
            for b in cross:
                if run(dict(os.environ, VO_BIN=b), path) != out:
                    fails += 1
                    print(f"FAIL {b} differs from {VO} on {path}")
    finally:
        for name in os.listdir(tmp):
            os.remove(os.path.join(tmp, name))
        os.rmdir(tmp)
    print(f"regex_diff: {len(pairs)} pairs, {fails} differences" + (f", {len(cross)} builds compared" if cross else "")
          + (f" ({slow} patterns skipped: Python took over 0.2 s)" if slow else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
