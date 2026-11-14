#!/usr/bin/env python3
"""Holds `vo doc` to the checker (spec 007 SC-002, research R12).

For every item `vo doc --json` lists (builtins and standard-library items):
- a call written exactly as documented, with a value of each documented type, checks clean;
- for each parameter, a call with a wrong-typed argument and a call with that argument missing
  are both rejected;
- (library items) the example `EXPR is VALUE` checks and prints exactly VALUE.
Also times `vo doc --json` with no NAME (must be under 1 s).
"""
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
TVARS = {"T": "*", "U": "*", "K": {"int", "str"}, "C": {"int", "float", "str"}}
BUILTIN_EFFECTS = {"print": "console", "write": "console", "eprint": "console",
                   "read_line": "input", "read_all": "input", "read_file": "file.read",
                   "write_file": "file.write", "now_ms": "clock", "random_int": "random",
                   "env": "env"}
TEST_ONLY = {"expect", "expect_eq"}
TYPE_PATHS = {}  # record/variant name -> its import path
WRONG = [('"a"', "str"), ("1", "int"), ("true", "bool")]


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
        m = re.fullmatch(r"(list|map|result)\[(.*)\]", t)
        if m:
            if m.group(1) == "result":
                return "ok()" if m.group(2) == "void" else f"ok({self.value(m.group(2), tv)})"
            return f"new {m.group(1)}[{self.inst(m.group(2), tv)}]"
        m = re.fullmatch(r"fn\((.*?)\)(?: -> (.*))?", t)
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
        p = subprocess.run([VO, "run", "--seed", "1", f], capture_output=True, text=True)
        return p.returncode, p.stdout, p.stderr


def program(imports, effects, call, void, test_only):
    imports = list(imports) + [q for n, q in TYPE_PATHS.items()
                               if q not in imports and re.search(rf"\b{n}\b", call)]
    head = "".join(f"import {i};\n" for i in imports)
    uses = f" uses {', '.join(effects)}" if effects else ""
    stmt = f"{call};" if void else f"let r = {call};"
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

    def probe(label, imports, effects, params, ret, tvars, test_only=False, name=None, rows=()):
        nonlocal calls
        tv = {k: "int" for k in tvars}
        void = ret in (None, "void")
        args = [vals.value(t, tv) for _, t in params]
        ok, out = check(program(imports, effects, f"{name}({', '.join(args)})", void, test_only))
        calls += 1
        if not ok:
            failures.append(f"{label}: the documented call is rejected\n{out}")
        for i, (pname, t) in enumerate(params):
            others = {x for j, (_, u) in enumerate(params) if j != i for x in re.findall(r"\b[A-Z]\b", u)}
            over = [ps[i][1] for ps in rows if len(ps) == len(params) and ps != params]
            w = vals.wrong(t, tv, tvars, others, over)
            if w is not None:
                bad = args[:i] + [w] + args[i + 1:]
                ok, _ = check(program(imports, effects, f"{name}({', '.join(bad)})", void, test_only))
                calls += 1
                if ok:
                    failures.append(f"{label}: a wrong type for `{pname}` ({w}) checks")
            fewer = args[:i] + args[i + 1:]
            ok, _ = check(program(imports, effects, f"{name}({', '.join(fewer)})", void, test_only))
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
            eff = [BUILTIN_EFFECTS[name]] if name in BUILTIN_EFFECTS else []
            probe(f"builtin {name}{b['signature']}", [], eff, params, ret, TVARS,
                  name in TEST_ONLY, name, sigs[name])

    examples = 0
    for module, rs in items.items():
        every = [f"{module}.{r['name']}" for r in rs]
        for r in rs:
            label = f"{module}.{r['name']}"
            if r["item"] == "fn":
                tvars = {t.split(":")[0].strip(): "*" for t in r["tparams"]}
                params = [(p["name"], p["type"]) for p in r["params"]]
                probe(label, [label], r["effects"], params, r["returns"], tvars, name=r["name"])
            ex = r.get("example")
            if not ex or " is " not in ex:
                failures.append(f"{label}: no example `EXPR is VALUE`")
                continue
            expr, value = ex.rsplit(" is ", 1)
            examples += 1
            if value.startswith('"'):
                show, want = expr, json.loads(value)
            else:
                show, want = f"str({expr})", value
            eff = sorted(set(["console"] + r.get("effects", [])),
                         key=["console", "input", "file.read", "file.write", "net", "clock", "random", "env"].index)
            src = "".join(f"import {i};\n" for i in every) + \
                f"\nfn main() uses {', '.join(eff)} {{\n    print({show});\n}}\n"
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
