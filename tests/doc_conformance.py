#!/usr/bin/env python3
"""Holds `vo doc` to the checker (spec 007 SC-002, research R12).

For every item `vo doc --json` lists (builtins and standard-library items):
- a call written exactly as documented, with a value of each documented type, checks clean;
- for each parameter, a call with a wrong-typed argument and a call with that argument missing
  are both rejected;
- (library items) the example `EXPR is VALUE` checks and prints exactly VALUE.
Also times `vo doc --json` with no NAME (must be under 1 s).
"""
import glob
import json
import os
import re
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

# Builtin type variables and what they range over ("*": anything).
TVARS = {"T": "*", "U": "*", "A": "*", "K": {"int", "str"}, "C": {"int", "float", "str"}}
# spec 023: builtins whose type comes from their place (ok, err, todo) or that only work in a `prop`
# (assume): documented for vo doc and hover, but a call on its own cannot be probed
CONTEXT_TYPED = {"ok", "err", "todo", "assume"}
BUILTIN_EFFECTS = {"print": "console", "write": "console", "eprint": "console",
                   "read_line": "input", "read_all": "input", "read_file": "file.read",
                   "write_file": "file.write", "now_ms": "clock", "random_int": "random",
                   "env": "env"}
TEST_ONLY = {"expect", "expect_eq"}
TYPE_PATHS = {"from_text": "std.bytes.from_text"}  # record/variant name -> its import path (spec 021: bytes values)
WRONG = [('"a"', "str"), ("1", "int"), ("true", "bool")]
# spec 014: the built-in collections are created with `new ordered_map[K, V]` etc.
COLLECTIONS = {"ordered_map", "ordered_set", "priority_queue", "deque"}


def doc(*args):
    p = subprocess.run([VO, "doc", "--json", *args], capture_output=True, text=True)
    return [r for r in map(json.loads, p.stdout.splitlines()) if r["kind"] == "doc"]


def split_top(s, sep=","):
    out, depth, cur = [], 0, ""
    for c in s:
        if c in "[(":
            depth += 1
        elif c in "])":
            depth -= 1
        if c == sep and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_builtin_sig(sig):
    """`(a: T, b: U) -> R` into ([(name, type)], R or None)."""
    depth = 0
    for i, c in enumerate(sig):
        depth += c in "(["
        depth -= c in ")]"
        if depth == 0:
            break
    params = []
    for p in split_top(sig[1:i]):
        name, ty = p.split(":", 1)
        params.append((name.strip(), ty.strip()))
    rest = sig[i + 1:].strip()
    return params, (rest[2:].strip() if rest.startswith("->") else None)


class Values:
    def __init__(self, types):
        self.types = types  # record/variant name -> doc record

    def inst(self, t, tv):
        return re.sub(r"\b[A-Z]\b", lambda m: tv.get(m.group(0), m.group(0)), t)

    def value(self, t, tv):
        t = t.strip()
        if "|" in t:
            t = t.split("|")[0].strip()
        if t in tv:
            t = tv[t]
        if t == "int":
            return "1"
        if t == "float":
            return "1.5"
        if t == "bool":
            return "true"
        if t == "str":
            return '"a"'
        if t == "bytes":  # spec 021: no literal
            return 'from_text("a")'
        m = re.fullmatch(r"(\w+)\[(.*)\]", t)
        if m and m.group(1) in COLLECTIONS:
            return f"new {self.inst(t, tv)}"
        m = re.fullmatch(r"(list|map|result)\[(.*)\]", t)
        if m:
            if m.group(1) == "result":
                return "ok()" if m.group(2) == "void" else f"ok({self.value(m.group(2), tv)})"
            return f"new {m.group(1)}[{self.inst(m.group(2), tv)}]"
        m = re.fullmatch(r"fn\((.*?)\)(?: -> (.*?))?(?: uses .*)?", t)  # a pure sample fits `uses E`
        if m:
            ps = ", ".join(f"p{i}: {self.inst(a, tv)}" for i, a in enumerate(split_top(m.group(1))))
            if m.group(2):
                r = self.inst(m.group(2), tv)
                return f"fn({ps}) -> {r} {{ return {self.value(r, tv)}; }}"
            return f"fn({ps}) {{ }}"
        d = self.types[t]
        if d["item"] == "record":
            fs = ", ".join(f'{f["name"]}: {self.value(f["type"], tv)}' for f in d["fields"])
            return f"new {t} {{ {fs} }}"
        c = d["cases"][0]
        return f"{t}.{c['name']}" + (f"({self.value(c['type'], tv)})" if c["type"] else "")

    def wrong(self, t, tv, tvars, others, overloads=()):
        """A value whose type cannot be t (nor any overload's type there), or None when t
        accepts anything."""
        t = t.strip()
        alts = [a.strip() for u in (t, *overloads) for a in u.split("|")]
        allowed = set()
        for a in alts:
            if a in tvars:
                if a not in others and tvars[a] == "*":
                    return None  # a lone type parameter takes any value
                allowed |= {tv[a]} if a in others or tvars[a] == "*" else tvars[a]
            else:
                allowed.add(a)
        for v, ty in WRONG:
            if ty not in allowed:
                return v
        return None


def check(src):
    with tempfile.TemporaryDirectory() as d:
        f = os.path.join(d, "probe.vo")
        open(f, "w").write(src)
        p = subprocess.run([VO, "check", "--json", f], capture_output=True, text=True)
        return p.returncode == 0, p.stdout


def run(src):
    with tempfile.TemporaryDirectory() as d:
        f = os.path.join(d, "probe.vo")
        open(f, "w").write(src)
        # spec 022: a fixed clock, as vo test uses, so clock examples (sleep) are exact and do not wait
        p = subprocess.run([VO, "run", "--seed", "1", "--clock-ms", "0", f], capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        return p.returncode, p.stdout, p.stderr


# spec 017: `process` needs scopes (E0512): the programs the probes ("a") and the examples run
PROCESS_SCOPE = 'process("a", "no-such-program.vo-doc")'


def scoped(effect):
    return PROCESS_SCOPE if effect == "process" else effect


def program(imports, effects, call, void, test_only, ret=None):
    text = call + " " + (ret or "")
    imports = list(imports) + [q for n, q in TYPE_PATHS.items()
                               if q not in imports and re.search(rf"\b{n}\b", text)]
    head = "".join(f"import {i};\n" for i in imports)
    uses = f" uses {', '.join(map(scoped, effects))}" if effects else ""
    stmt = f"{call};" if void else f"let r{': ' + ret if ret else ''} = {call};"
    if test_only:
        return f'{head}\ntest "probe"{uses} {{\n    {stmt}\n}}\n'
    return f"{head}\nfn probe(){uses} {{\n    {stmt}\n}}\n"


def main():
    t0 = time.time()
    listing = doc()
    elapsed = time.time() - t0
    builtins = [r for r in listing if r["item"] == "builtin"]
    modules = [r["name"] for r in listing if r["item"] == "module"]
    items = {m: doc(m) for m in modules}
    types = {r["name"]: r for rs in items.values() for r in rs if r["item"] != "fn"}
    vals = Values(types)
    TYPE_PATHS.update({r["name"]: f"{m}.{r['name']}" for m, rs in items.items() for r in rs
                       if r["item"] != "fn"})
    failures, calls = [], 0
    # spec 010: in std/, the exported items are exactly the documented ones
    for path in sorted(os.listdir(os.path.join(ROOT, "std"))):
        lines = open(os.path.join(ROOT, "std", path), encoding="utf-8").read().split("\n")
        for i, line in enumerate(lines):
            m = re.match(r"(export )?(fn|record|variant) (\w+)", line)
            if m and bool(m.group(1)) != (i > 0 and lines[i - 1].startswith("#")):
                failures.append(f"std/{path}:{i + 1}: `{m.group(3)}` is "
                                + ("exported without a doc comment" if m.group(1) else "documented but not exported"))

    def probe(label, imports, effects, params, ret, tvars, test_only=False, name=None, rows=()):
        nonlocal calls
        tv = {k: "int" for k in tvars}
        void = ret in (None, "void")
        args = [vals.value(t, tv) for _, t in params]
        # spec 014: a type parameter only the result mentions comes from a declared type
        free = [x for x in re.findall(r"\b[A-Z]\b", ret or "") if not any(re.search(rf"\b{x}\b", t) for _, t in params)]
        annot = vals.inst(ret, tv) if free and not void else None
        ok, out = check(program(imports, effects, f"{name}({', '.join(args)})", void, test_only, annot))
        calls += 1
        if not ok:
            failures.append(f"{label}: the documented call is rejected\n{out}")
        for i, (pname, t) in enumerate(params):
            others = {x for j, (_, u) in enumerate(params) if j != i for x in re.findall(r"\b[A-Z]\b", u)}
            over = [ps[i][1] for ps in rows if len(ps) == len(params) and ps != params]
            w = vals.wrong(t, tv, tvars, others, over)
            if w is not None:
                bad = args[:i] + [w] + args[i + 1:]
                ok, _ = check(program(imports, effects, f"{name}({', '.join(bad)})", void, test_only, annot))
                calls += 1
                if ok:
                    failures.append(f"{label}: a wrong type for `{pname}` ({w}) checks")
            fewer = args[:i] + args[i + 1:]
            ok, _ = check(program(imports, effects, f"{name}({', '.join(fewer)})", void, test_only, annot))
            calls += 1
            if ok:
                failures.append(f"{label}: a call without `{pname}` checks")

    sigs = {}
    for b in builtins:
        for name in [n.strip() for n in b["name"].split(",")]:
            sigs.setdefault(name, []).append(parse_builtin_sig(b["signature"])[0])
    for b in builtins:
        params, ret = parse_builtin_sig(b["signature"])
        for name in [n.strip() for n in b["name"].split(",")]:
            if name in CONTEXT_TYPED:
                continue
            eff = [BUILTIN_EFFECTS[name]] if name in BUILTIN_EFFECTS else []
            probe(f"builtin {name}{b['signature']}", [], eff, params, ret, TVARS,
                  name in TEST_ONLY, name, sigs[name])

    # spec 023: everything a user can hover is documented: every builtin row has notes, every named
    # builtin is in vo doc, and every field and case of an exported std record or variant has a comment
    for b in builtins:
        if not b.get("description"):
            failures.append(f"builtin {b['name']}: no notes in the reference's table (hover shows none)")
    table = open(os.path.join(ROOT, "src", "check_expr.c")).read()
    names_src = table[table.index("builtin_names[B_COUNT + 1] = {"):]
    names_src = names_src[:names_src.index("};")]
    documented = {n.strip() for b in builtins for n in b["name"].split(",")}
    for n in re.findall(r'"([a-z_]+)"', names_src):
        if not n.startswith("__") and n not in documented:
            failures.append(f"builtin {n}: not in any builtin table of the reference (vo doc and hover miss it)")
    for path in sorted(glob.glob(os.path.join(ROOT, "std", "*.vo"))):
        lines = open(path).read().split("\n")
        inside = None
        for i, line in enumerate(lines):
            m = re.match(r"export (record|variant) (\w+)", line)
            if m:
                inside = m.group(2)
                continue
            if inside and line.startswith("}"):
                inside = None
            elif inside and line.strip() and not line.strip().startswith("#") \
                    and not lines[i - 1].strip().startswith("#"):
                failures.append(f"std/{os.path.basename(path)}:{i + 1}: `{inside}.{line.strip().rstrip(';')}` "
                                "has no comment (hover shows none)")

    examples = 0
    for module, rs in items.items():
        every = [f"{module}.{r['name']}" for r in rs]
        for r in rs:
            label = f"{module}.{r['name']}"
            if r["item"] == "fn":
                tvars = {t.split(":")[0].strip(): "*" for t in r["tparams"]}
                params = [(p["name"], p["type"]) for p in r["params"]]
                probe(label, [label], [e for e in r["effects"] if not re.fullmatch(r"[A-Z]", e)], params,
                      r["returns"], tvars, name=r["name"])
            ex = r.get("example")
            if not ex or " is " not in ex:
                failures.append(f"{label}: no example `EXPR is VALUE`")
                continue
            expr, value = ex.rsplit(" is ", 1)
            setup = ""
            if expr.startswith("let ") and "; " in expr:  # spec 014: statements first (`let s = ...; push(s, 1); EXPR`)
                setup, expr = expr.rsplit("; ", 1)
                setup += ";\n    "
            examples += 1
            if value.startswith('"'):
                show, want = expr, json.loads(value)
            else:
                show, want = f"str({expr})", value
            eff = sorted(set(["console"] + [e for e in r.get("effects", []) if not re.fullmatch(r"[A-Z]", e)]),
                         key=["console", "input", "file.read", "file.write", "file.delete", "net", "clock", "random",
                              "env", "serve", "process"].index)  # effect parameters (E) are not effects to declare
            # spec 021: an example in another module may make its bytes with std.bytes functions
            extra = [f"std.bytes.{r2['name']}" for r2 in items.get("std.bytes", [])
                     if module != "std.bytes" and re.search(r"\b" + r2["name"] + r"\(", ex)]
            src = "".join(f"import {i};\n" for i in every + extra) + \
                f"\nfn main() uses {', '.join(map(scoped, eff))} {{\n    {setup}print({show});\n}}\n"
            code, out, err = run(src)
            if code != 0 or out != want + "\n":
                failures.append(f"{label}: example `{ex}` printed {out!r} (exit {code})\n{err}")

    for f in failures:
        print("FAIL", f)
    print(f"doc_conformance: {len(builtins)} builtin rows, {sum(map(len, items.values()))} library items, "
          f"{calls} checked calls, {examples} examples, {len(failures)} failures; "
          f"vo doc --json took {elapsed:.2f} s")
    if elapsed >= 1.0:
        print("FAIL vo doc --json took 1 s or more")
        return 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
