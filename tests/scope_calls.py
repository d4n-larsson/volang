#!/usr/bin/env python3
"""Spec 062: which declarations on a call path bind a read, on random call paths (FR-001 to FR-006).

Each program is a chain of functions called by name, outermost first: some without an effect
parameter (they take a function of a fixed type), then some with one (they take `f: fn() -> str uses E`).
Each has scopes of its own (some of a/, b/, c/) or none. `main` declares all three and passes a
function that reads the path T. The chain ends in one of:

  own    the innermost function reads the path P itself
  call   the innermost function calls what it was given
  wrap   a function with an effect parameter hands on its own `fn() => rd(p) + "+" + f()` in place of
         `f` (the innermost one calls it itself), and the innermost function calls what it was given

The expected outcome comes from specs/062-generic-own-scope/contracts/call-path.md, not from the
run time: a function's own code is bound by it and by every function that called it by name; what was
passed in under an effect parameter is bound by the functions without one, and not by those with one.
The first function, from the read outwards, whose scopes do not hold the path is the one R0010 names.

  scope_calls.py [--iterations N] [--seed S]
"""
import argparse, os, random, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
DIRS = ["a", "b", "c"]
PRE = '''fn rd(p: str) -> str uses file.read {
    match read_file(p) {
        case ok(s) {
            return trim(s);
        }
        case err(_) {
            return "err";
        }
    }
}

fn first() -> str {
    return "P";
}

fn second() -> str {
    return "T";
}
'''


def uses(scope):
    return "file.read" if scope is None else ", ".join(f'file.read("{d}/")' for d in sorted(scope))


def build(rng):
    nplain, ngen = rng.randint(0, 3), rng.randint(0, 3)
    if nplain + ngen == 0:
        ngen = 1
    n = nplain + ngen
    scopes, held = [], set()  # innermost first: a scoped caller covers the scoped functions it calls
    for _ in range(n):
        if rng.random() < 0.3:
            scopes.append(None)
        else:
            held = held | set(rng.sample(DIRS, rng.randint(0, 2))) or {rng.choice(DIRS)}
            scopes.append(set(held))
    scopes.reverse()  # outermost first
    names = [f"p{i + 1}" for i in range(nplain)] + [f"g{i + 1}" for i in range(ngen)]
    end = rng.choice(["own", "call", "wrap"] if ngen else ["own", "call"])
    wrap = rng.randrange(nplain, n) if end == "wrap" else -1
    out = [PRE]
    for i, name in enumerate(names):
        gen, last = i >= nplain, i == n - 1
        given = "f" if gen else "k"
        body = ""
        if i == wrap:
            body = f'    let l = fn() => rd(p) + "+" + {given}();\n'
            given = "l"
        if last:
            ret = "rd(p)" if end == "own" else f"{given}()"
        else:
            ret = f"{names[i + 1]}(p, {given})"
        head = (f"fn {name}[E: effects](p: str, f: fn() -> str uses E) -> str uses E, {uses(scopes[i])}" if gen
                else f"fn {name}(p: str, k: fn() -> str uses file.read) -> str uses {uses(scopes[i])}")
        out.append(f"\n{head} {{\n{body}    return {ret};\n}}\n")
    p, t = rng.choice(DIRS), rng.choice(DIRS)
    out.append(f'\nfn main() uses console, {uses(set(DIRS))} {{\n    print({names[0]}(first(), fn() => rd(second())));\n}}\n')
    src = "".join(out).replace('"P"', f'"{p}/f"').replace('"T"', f'"{t}/f"')
    # what binds each read, from the read outwards (the contract)
    by_name = lambda upto: [names[j] for j in range(upto, -1, -1)]          # a function and all its callers
    plains = [names[j] for j in range(nplain - 1, -1, -1)]                    # the passed-in function's binders
    reads = []
    if end == "own":
        reads.append((p, by_name(n - 1)))
    if end == "wrap":
        reads.append((p, by_name(wrap)))
    if end in ("call", "wrap"):
        reads.append((t, plains))
    want = None
    for d, binders in reads:
        for name in binders:
            s = scopes[names.index(name)]
            if s is not None and d not in s:
                want = name
                break
        if want:
            break
    return src, want, f"{nplain} plain, {ngen} generic, {end}" + (f" at {names[wrap]}" if wrap >= 0 else "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=300)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    rng = random.Random(a.seed)
    bad = refused = skipped = 0
    with tempfile.TemporaryDirectory() as tmp:
        for d in DIRS:
            os.mkdir(os.path.join(tmp, d))
            open(os.path.join(tmp, d, "f"), "w").write(d)
        for i in range(a.iterations):
            src, want, shape = build(rng)
            open(os.path.join(tmp, "prog.vo"), "w").write(src)
            r = subprocess.run([VO, "run", "prog.vo"], cwd=tmp, capture_output=True, text=True, timeout=60)
            if r.returncode == 65:  # the checker's own rules on scopes (not what is tested here)
                skipped += 1
                continue
            m = re.search(r"runtime error\[R0010\]: .* is not allowed: `(\w+)` declares", r.stderr)
            got = m.group(1) if m else None if r.returncode == 0 else f"exit {r.returncode}"
            refused += got is not None
            if got != want:
                bad += 1
                if bad <= 5:
                    print(f"MISMATCH (iteration {i}, {shape}): want {want}, got {got}\n{src}\n{r.stderr[:300]}")
    print(f"scope_calls: {a.iterations} programs ({refused} refused, {skipped} not checked), {bad} mismatches")
    sys.exit(1 if bad or skipped > a.iterations // 10 else 0)


main()
