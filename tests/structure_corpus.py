#!/usr/bin/env python3
"""Spec 027 SC-002 and SC-003: the structure rules on the corpus.

- every example program under examples/ checks with no structure warning (W07xx);
- the standard library (through a program importing every module, with VO_STRUCTURE_STD=1) has none
  that is not accepted with an `@allow`;
- seeded violations, five per rule (W0701-W0705), generated here: every one is reported with its code;
- well-structured programs (the examples, plus the clean programs generated here, at least 10 in all):
  none reported.
(The runnable examples of the reference and the site are checked warning-free by tests/doc_examples.py.)
"""
import glob
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
STD = ["json.parse_json", "time.sleep", "text.lines", "random.unit", "http.get", "collections.first", "fs.info",
       "process.run", "math.sqrt", "bytes.from_text", "regex.regex", "sqlite.open_memory",
       "sse.parse_events", "websocket.ws_connect"]


def codes(entry, env=None):
    r = subprocess.run([VO, "check", "--json", os.path.basename(entry)], cwd=os.path.dirname(entry),
                       capture_output=True, text=True, timeout=120, env=env)
    recs = [json.loads(l) for l in r.stdout.splitlines() if l.startswith("{")]
    return r.returncode, [x["code"] for x in recs if x.get("kind") == "diagnostic"], recs


def write(d, files):
    for name, text in files.items():
        p = os.path.join(d, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.write(text)
    return os.path.join(d, "main.vo")


def helpers(n, start=1, export=False):
    return "".join(f"{'export ' if export else ''}fn s{i}(x: int) -> int {{\n    return x + {i};\n}}\n\n"
                   for i in range(start, start + n))


def lets(n, indent="    "):
    return "".join(f"{indent}let v{i} = {'1' if i == 0 else f'v{i - 1} + {i}'};\n" for i in range(n))


def seeds():
    """{rule: [files dict, ...]}, five programs per rule."""
    s = {}
    s["W0701"] = [
        {"main.vo": "fn main() uses console {\n" + lets(16) + "    print(str(v15));\n}\n"},
        {"main.vo": "fn main() uses console {\n    print(\"start\");\n" + lets(8) + "    print(str(v7));\n"
         + lets(8).replace("v", "w").replace("let w0 = 1", "let w0 = v7") + "    print(str(w7));\n}\n"},
        {"main.vo": "fn main() uses console {\n    var total = 0;\n    for i in 0..10 {\n" + lets(14, "        ")
         + "        set total = total + v13;\n    }\n    print(str(total));\n}\n"},
        {"main.vo": "fn report(xs: list[int]) uses console {\n" + lets(15) + "    print(str(v14 + len(xs)));\n}\n\n"
         "fn main() uses console {\n    report([1]);\n}\n"},
        {"main.vo": "fn main() -> result[void] uses console {\n    let n = try parse_int(\"4\");\n" + lets(15)
         + "    print(str(n + v14));\n    return ok();\n}\n"},
    ]
    main1 = 'fn main() uses console {\n    print(str(s1(0)));\n}\n'
    s["W0702"] = [
        {"main.vo": helpers(134) + main1},
        {"main.vo": "# comments do not count\n\n" + helpers(130) + "fn extra() -> int {\n" + lets(12) + "    return v11;\n}\n\n" + main1},
        {"main.vo": "import parts;\n\n" + helpers(134) + main1.replace("s1(0)", "s1(parts.p1())"),
         "parts.vo": "export fn p1() -> int {\n    return 1;\n}\n"},
        {"main.vo": main1.replace("s1(0)", "big(1)") + "\nfn big(x: int) -> int {\n    var t = x;\n"
         + "".join(f"    set t = s{i}(t);\n" for i in range(1, 136)) + "    return t;\n}\n\n" + helpers(140)},
        {"main.vo": helpers(132) + main1 + "\ntest \"one\" {\n    expect_eq(s1(1), 2);\n}\n"},
    ]

    def long_body(n, start=1):
        return "    var v = 0;\n" + "".join(f"    set v = s{i}(v);\n" for i in range(start, start + n - 2)) + "    return v;\n"
    s["W0703"] = [
        {"main.vo": helpers(60) + "fn long() -> int {\n" + long_body(51) + "}\n\n" + main1.replace("s1(0)", "long()")},
        {"main.vo": helpers(60) + "fn long() -> int uses console {\n    print(\"x\");\n" + long_body(51) + "}\n\n"
         "fn main() uses console {\n    print(str(long()));\n}\n"},
        {"main.vo": helpers(60) + "test \"long\" {\n    var v = 0;\n" + "".join(f"    set v = s{i}(v);\n" for i in range(1, 51))
         + "    expect(v > 0);\n}\n"},
        {"main.vo": helpers(60) + "prop \"long\" (x: int) {\n    assume(x > 0 and x < 1000);\n    var v = x;\n"
         + "".join(f"    set v = s{i}(v);\n" for i in range(1, 50)) + "    expect(v > x);\n}\n"},
        {"main.vo": helpers(60) + "fn long() -> int {\n    let f: fn(int) -> int = fn(x) {\n"
         + "".join(f"        let a{i} = s{i}(x);\n" for i in range(1, 50)) + "        return a49;\n    };\n    return f(1);\n}\n\n"
         + main1.replace("s1(0)", "long()")},
    ]
    dup = ("    let tax = base * 25 / 100;\n    let fee = 3;\n    var total = base + tax;\n    set total = total + fee;\n"
           "    if total > 1000 {\n        set total = total - 10;\n    }\n    return total;\n")
    dup2 = dup.replace("base", "amount").replace("tax", "vat").replace("fee", "charge").replace("total", "sum")
    s["W0704"] = [
        {"main.vo": f"fn a(base: int) -> int {{\n{dup}}}\n\nfn b(amount: int) -> int {{\n{dup2}}}\n\n"
         "fn main() uses console {\n    print(str(a(1) + b(2)));\n}\n"},
        {"main.vo": f"import other;\n\nfn a(base: int) -> int {{\n{dup}}}\n\nfn main() uses console {{\n    print(str(a(1) + other.b(2)));\n}}\n",
         "other.vo": f"export fn b(amount: int) -> int {{\n{dup2}}}\n"},
        {"main.vo": f"fn a(base: int) -> int {{\n{dup}}}\n\nfn b(base: int) -> int {{\n    if base > 0 {{\n"
         + "".join("    " + l + "\n" for l in dup.rstrip("\n").split("\n")) + "    }\n    return 0;\n}\n\n"
         "fn main() uses console {\n    print(str(a(1) + b(2)));\n}\n"},
        {"main.vo": f"fn a(base: int) -> int {{\n{dup}}}\n\nfn b(amount: int) -> int {{\n{dup2}}}\n\nfn c(base: int) -> int {{\n{dup}}}\n\n"
         "fn main() uses console {\n    print(str(a(1) + b(2) + c(3)));\n}\n"},
        {"main.vo": "fn shout(x: int) -> int uses console {\n    print(str(x));\n    return x;\n}\n\n"
         "fn quiet(x: int) -> int {\n    return x;\n}\n\n"
         "fn run_a(f: fn(int) -> int uses console) -> int uses console {\n    let a = f(1);\n    return a + 1;\n}\n\n"
         "fn run_b(f: fn(int) -> int) -> int {\n    let a = f(1);\n    return a + 1;\n}\n\n"
         "fn main() uses console {\n    print(str(run_a(shout) + run_b(quiet)));\n}\n"},
    ]
    money = "export fn cents(n: int) -> int {\n    return n * 100;\n}\n\nexport fn units(c: int) -> int {\n    return c / 100;\n}\n"
    imp = "import money;\n\nfn main() uses console {\n    print(str(money.units(money.cents(2))));\n}\n\n"
    s["W0705"] = [
        {"main.vo": imp + 'test "cents" {\n    expect_eq(money.cents(3), 300);\n}\n', "money.vo": money},
        {"main.vo": imp + 'test "round trip" {\n    expect_eq(money.units(money.cents(3)), 3);\n}\n', "money.vo": money},
        {"main.vo": imp + 'prop "round trip" (x: int) {\n    assume(x > 0 and x < 1000);\n    expect_eq(money.units(money.cents(x)), x);\n}\n',
         "money.vo": money},
        {"main.vo": imp + 'test "in a loop" {\n    for i in 0..3 {\n        expect_eq(money.units(money.cents(i)), i);\n    }\n}\n',
         "money.vo": money},
        {"main.vo": imp + 'test "through a function value" {\n    let f: fn(int) -> int = fn(x) => money.cents(x);\n'
         '    expect_eq(f(1), 100);\n}\n', "money.vo": money},
    ]
    return s


def clean():
    """Well-structured programs that no rule may report (with the examples, at least 10)."""
    money = "export fn cents(units: int) -> int {\n    return units * 100;\n}\n\ntest \"cents\" {\n    expect_eq(cents(3), 300);\n}\n"
    return [
        {"main.vo": "import money;\n\nfn main() uses console {\n    print(str(money.cents(2)));\n}\n", "money.vo": money},
        {"main.vo": "fn total(xs: list[int]) -> int {\n    var t = 0;\n    for x in xs {\n        set t = t + x;\n    }\n"
         "    return t;\n}\n\nfn main() uses console {\n    print(str(total([1, 2, 3])));\n}\n\ntest \"total\" {\n"
         "    expect_eq(total([1, 2]), 3);\n}\n"},
        {"main.vo": helpers(100) + "fn main() uses console {\n    print(str(s1(s2(0))));\n}\n"},
        {"main.vo": "fn main() uses console {\n" + lets(14) + "    print(str(v13));\n}\n"},
        {"main.vo": "fn each[E: effects](xs: list[int], f: fn(int) uses E) uses E {\n    for x in xs {\n        f(x);\n    }\n}\n\n"
         "fn main() uses console {\n    each([1, 2], fn(x) {\n        print(str(x));\n    });\n}\n"},
    ]


def main():
    fails = 0
    examples = sorted(glob.glob(os.path.join(ROOT, "examples", "*", "main.vo")) +
                      glob.glob(os.path.join(ROOT, "examples", "*", "*", "main.vo")))
    for e in examples:
        _, cs, _ = codes(e)
        w = [c for c in cs if c.startswith("W07")]
        if w:
            fails += 1
            print(f"FAIL {os.path.relpath(e, ROOT)}: {w}")
    with tempfile.TemporaryDirectory() as tmp:
        entry = write(os.path.join(tmp, "std"), {"main.vo": "".join(f"import std.{m};\n" for m in STD)
                                                  + "\nfn main() uses console {\n    print(\"x\");\n}\n"})
        _, cs, recs = codes(entry, dict(os.environ, VO_STRUCTURE_STD="1"))
        summary = next((r for r in recs if r.get("kind") == "summary"), {})
        if [c for c in cs if c.startswith("W07")]:
            fails += 1
            print(f"FAIL std: {cs}")
        accepted = summary.get("accepted", 0)
        found = {}
        for rule, progs in seeds().items():
            for i, files in enumerate(progs):
                rc, cs, _ = codes(write(os.path.join(tmp, f"{rule}_{i}"), files))
                ok = rc == 0 and rule in cs
                found[rule] = found.get(rule, 0) + ok
                if not ok:
                    fails += 1
                    print(f"FAIL seed {rule} #{i + 1}: exit {rc}, codes {cs}")
        cleans = clean()
        for i, files in enumerate(cleans):
            rc, cs, _ = codes(write(os.path.join(tmp, f"clean_{i}"), files))
            bad = [c for c in cs if c.startswith("W07") or c.startswith("E")]
            if rc != 0 or bad:
                fails += 1
                print(f"FAIL clean #{i + 1}: exit {rc}, codes {cs}")
    n_clean = len(examples) + len(cleans)
    if n_clean < 10:
        fails += 1
        print(f"FAIL only {n_clean} well-structured programs")
    print(f"structure_corpus: {len(examples)} examples clean, std {accepted} accepted, seeds "
          + ", ".join(f"{k} {v}/5" for k, v in sorted(found.items())) + f", {n_clean} clean programs, {fails} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
