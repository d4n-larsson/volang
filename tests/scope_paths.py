#!/usr/bin/env python3
"""Spec 050: scopes along the call path (SC-002).

  scope_paths.py [--count N] [--seed S]     N generated programs (default 300, seed 1; part of `make test`)
  scope_paths.py --time                      SC-005: 100,000 scoped reads at call depth 1, 50 and 1,000 (timing)

Each program is `main` and 1 to 6 functions calling each other in a line; the last one connects to
127.0.0.1 on port 1, 2 or 3 (nothing listens there) or reads a/x.txt, b/x.txt or c/x.txt. Every
function declares the effect without scopes or with some of the three; calls are plain, through an
anonymous function, or through a generic function that takes the callee as a value. The target is a
literal at the library call or a parameter passed down from `main`.

The model of the rule says what must happen:
  E0503   a function's scopes do not include its callee's, or those its callee needs in turn through
          plain calls (a function value's type holds only the callee's declaration), or a literal
          file target is outside the scopes of the function that contains the library call;
  R0010   otherwise, when some scoped function on the path excludes the target: it names the one
          nearest the operation;
  reached otherwise.
A run that reaches a target outside `main`'s scopes fails whatever the model says.
"""
import argparse
import json
import os
import random
import re
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN, natively or under qemu
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1

NET = ["127.0.0.1:1", "127.0.0.1:2", "127.0.0.1:3"]
FILE = ["a/", "b/", "c/"]


def clause(effect, scopes):
    if scopes is None:
        return effect
    return effect + "(" + ", ".join('"%s"' % s for s in sorted(scopes)) + ")"


def generate(rng):
    """One program: (source, kind, names, scopes per function, target index, literal, call styles)."""
    kind = rng.choice(["net", "file"])
    universe = NET if kind == "net" else FILE
    depth = rng.randint(1, 6)
    names = ["main"] + ["f%d" % i for i in range(1, depth + 1)]
    scopes = []
    for i in range(depth + 1):
        if rng.random() < (0.15 if i == 0 else 0.45):
            scopes.append(None)  # declared without scopes
        else:
            scopes.append(set(rng.sample(universe, rng.randint(1, 3))))
    target = rng.randrange(3)
    literal = rng.random() < 0.3
    effect = "net" if kind == "net" else "file.read"
    ptype = "int" if kind == "net" else "str"
    value = str(target + 1) if kind == "net" else '"%sx.txt"' % FILE[target]
    out = []
    if kind == "net":
        out += ["import std.tcp;", ""]
    out += ["fn via[E: effects](g: fn(%s) -> str uses E, p: %s) -> str uses E {" % (ptype, ptype),
            "    return g(p);", "}", ""]
    last = names[-1]
    arg = value if literal else "p"
    out.append("fn %s(p: %s) -> str uses %s {" % (last, ptype, clause(effect, scopes[-1])))
    if kind == "net":
        out += ['    match tcp.connect("127.0.0.1", %s, 1000) {' % arg,
                "        case ok(c) {", "            tcp.close_conn(c);", '            return "open " + str(p);', "        }",
                "        case err(e) {", '            return tcp.tcp_error_text(e) + " " + str(p);', "        }", "    }"]
    else:
        out += ["    match read_file(%s) {" % arg,
                "        case ok(t) {", '            return "read " + t + p;', "        }",
                "        case err(e) {", "            return e + p;", "        }", "    }"]
    out += ["}", ""]
    styles = ["plain"] * (depth + 1)  # how each function calls the next one; main calls f1 plainly
    for i in range(depth - 1, 0, -1):
        style = styles[i] = rng.choice(["plain", "lambda", "generic"])
        out.append("fn %s(p: %s) -> str uses %s {" % (names[i], ptype, clause(effect, scopes[i])))
        if style == "plain":
            out.append("    return %s(p);" % names[i + 1])
        elif style == "lambda":
            # spec 059: an anonymous function takes its types (and keeps its effects) from via's parameter
            out.append("    return via(fn(q) => %s(q), p);" % names[i + 1])
        else:
            out.append("    return via(%s, p);" % names[i + 1])
        out += ["}", ""]
    out += ["fn main() uses console, %s {" % clause(effect, scopes[0]), "    print(f1(%s));" % value, "}", ""]
    return "\n".join(out), kind, names, scopes, target, literal, styles


def model(kind, names, scopes, target, literal, styles):
    """('E0503', {function names}) | ('R0010', name) | ('reached', None)."""
    universe = NET if kind == "net" else FILE
    t = universe[target]
    narrow = set()
    # What each function needs: the scopes its callee declares and, through a plain call, what the
    # callee needs in turn (a function value's type holds only the callee's declaration).
    need = [set() for _ in names]
    for i in range(len(names) - 2, -1, -1):
        need[i] = set(scopes[i + 1] or ())
        if styles[i] == "plain":
            need[i] |= need[i + 1]
        if scopes[i] is not None and not need[i] <= scopes[i]:
            narrow.add(names[i])
    # (a literal host and port are not compared at check time today: only file targets are)
    if literal and kind == "file" and scopes[-1] is not None and t not in scopes[-1]:
        narrow.add('"%sx.txt"' % t)  # the diagnostic is on the literal
    if narrow:
        return "E0503", narrow
    for i in range(len(names) - 1, -1, -1):  # from the operation outwards
        if scopes[i] is not None and t not in scopes[i]:
            return "R0010", names[i]
    return "reached", None


def run_vo(args, cwd):
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, text=True, timeout=60 * SLOW)
    return p.returncode, p.stdout, p.stderr


def one(seed, work):
    rng = random.Random(seed)
    source, kind, names, scopes, target, literal, styles = generate(rng)
    path = os.path.join(work, "p.vo")
    with open(path, "w") as f:
        f.write(source)
    want, detail = model(kind, names, scopes, target, literal, styles)
    code, out, _ = run_vo(["check", "--json", "p.vo"], work)
    errors = [json.loads(l) for l in out.splitlines() if l.startswith("{")]
    errors = [d for d in errors if d.get("severity") == "error"]
    if want == "E0503":
        got = {d.get("text") for d in errors if d.get("code") == "E0503"}
        if code != 65 or got != detail or any(d.get("code") != "E0503" for d in errors):
            return "check: want E0503 on %s, got status %d %s" % (sorted(detail), code, [(d.get("code"), d.get("text")) for d in errors])
        return None
    if errors or code != 0:
        return "check: want no error, got status %d %s" % (code, [(d.get("code"), d.get("message")) for d in errors])
    code, out, err = run_vo(["run", "p.vo"], work)
    reached = code == 0 and ("refused" in out or out.startswith("read "))
    root = scopes[0]
    universe = NET if kind == "net" else FILE
    if reached and root is not None and universe[target] not in root:
        return "run: reached %s outside main's scopes %s" % (universe[target], sorted(root))
    if want == "R0010":
        m = re.search(r"R0010\]: .* is not allowed: `(\w+)` declares", err)
        if code != 70 or not m or m.group(1) != detail:
            return "run: want R0010 naming %s, got status %d: %s%s" % (detail, code, out.strip()[:80], err.strip()[:160])
        return None
    if not reached:
        return "run: want the target reached, got status %d: %s%s" % (code, out.strip()[:80], err.strip()[:160])
    return None


def timing(work):
    """SC-005: 100,000 scoped reads at depth 1, 50 and 1,000; the limit is on 1,000 against 1."""
    template = """fn deep(n: int, p: str) -> int {
    if n > 0 {
        return deep(n - 1, p);
    }
    var total = 0;
    for i in 0..100000 {
        match read_file(p) {
            case ok(t) {
                set total = total + len(t);
            }
            case err(_) {
            }
        }
    }
    return total;
}

fn main() uses console, file.read("a/") {
    print(str(deep(DEPTH, "a/x.txt")));
}
"""
    template = template.replace("fn deep(n: int, p: str) -> int {", "fn deep(n: int, p: str) -> int uses file.read {")
    times = []
    for depth in (1, 50, 1000):
        with open(os.path.join(work, "t.vo"), "w") as f:
            f.write(template.replace("DEPTH", str(depth)))
        best = None
        for _ in range(3):
            t0 = time.time()
            p = subprocess.run([VO, "run", "--timeout", "600", "t.vo"], cwd=work, capture_output=True, text=True)
            dt = time.time() - t0
            if p.returncode != 0:
                print("FAIL timing depth %d: status %d %s" % (depth, p.returncode, p.stderr[:300]))
                return 1
            best = dt if best is None else min(best, dt)
        times.append(best)
        print("scope_paths: 100,000 scoped reads at depth %d: %.3f s" % (depth, best))
    ratio = times[2] / times[0]
    print("scope_paths: ratio %.2f (limit 2.00)" % ratio)
    return 0 if ratio <= 2.0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--time", action="store_true")
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as work:
        for d in FILE:
            os.mkdir(os.path.join(work, d))
            with open(os.path.join(work, d, "x.txt"), "w") as f:
                f.write("x")
        if a.time:
            return timing(work)
        failures = 0
        for seed in range(a.seed, a.seed + a.count):
            problem = one(seed, work)
            if problem:
                failures += 1
                if failures <= 10:
                    print("FAIL seed %d: %s" % (seed, problem))
                    keep = os.path.join(ROOT, "build", "scope-paths-%d.vo" % seed)
                    os.makedirs(os.path.dirname(keep), exist_ok=True)
                    with open(os.path.join(work, "p.vo")) as src, open(keep, "w") as dst:
                        dst.write(src.read())
        print("scope_paths: %d programs, %d failures" % (a.count, failures))
        return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
