#!/usr/bin/env python3
"""Spec 042: what the string changes need a reference implementation or size for.

  strings.py                 every group (part of `make test`)
  strings.py --group NAME    one group: case, exponents, loop

case       upper and lower of every code point against Python's simple mappings in the covered blocks,
           identity elsewhere; src/case_table.h equals what tools/gen_case.py prints
exponents  10,000 random float literals with exponents against Python's float()
loop       `for c in s` against chars(s) on random text; a 10 MiB text under a 32 MiB limit; its speed
"""
import os
import random
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gen_case  # noqa: E402

failures = []
cases = 0

CASE = """fn main() uses console {
    for cp in 0..1114112 {
        if cp < 55296 or cp > 57343 {
            let c = chr(cp);
            let u = upper(c);
            let l = lower(c);
            if u != c {
                print("u " + str(cp) + " " + str(ord(u)));
            }
            if l != c {
                print("l " + str(cp) + " " + str(ord(l)));
            }
        }
    }
}
"""

LOOP = """import std.fs.each_line;

record Seen {
    lines: int;
    bad: int;
}

fn same(s: str) -> bool {
    let want = chars(s);
    var i = 0;
    for c in s {
        if i >= len(want) or want[i] != c {
            return false;
        }
        set i = i + 1;
    }
    return i == len(want);
}

fn main() uses console, file.read("./") {
    let seen = new Seen { lines: 0, bad: 0 };
    let done = each_line("texts.txt", fn(line: str) -> result[bool] {
        set seen.lines = seen.lines + 1;
        if not same(line) {
            set seen.bad = seen.bad + 1;
        }
        return ok(true);
    });
    match done {
        case ok() {
            print(str(seen.lines) + " " + str(seen.bad));
        }
        case err(m) {
            print(m);
        }
    }
}
"""

BIG = """fn main() uses console, clock, file.read("./") {
    match read_file("big.txt") {
        case ok(s) {
            let start = now_ms();
            var n = 0;
            var wide = 0;
            for c in CHARS {
                set n = n + 1;
                if len(c) > 1 {
                    set wide = wide + 1;
                }
            }
            print(str(n) + " " + str(wide) + " " + str(now_ms() - start));
        }
        case err(m) {
            print(m);
        }
    }
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def run(d, src, extra=(), name="p.vo", timeout=600):
    with open(os.path.join(d, name), "w") as f:
        f.write(src)
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW)] + list(extra) + [name], cwd=d, capture_output=True, text=True,
                       timeout=timeout * SLOW)
    return r.returncode, r.stdout, r.stderr


def covered(cp):
    return any(lo <= cp <= hi for lo, hi in gen_case.RANGES)


def group_case(d):
    want = {}
    for cp in range(0x110000):
        if 0xD800 <= cp <= 0xDFFF:
            continue
        c = chr(cp)
        if cp < 128:
            u, low = c.upper(), c.lower()
        elif covered(cp):
            u, low = c.upper(), c.lower()
            u = u if len(u) == 1 else c
            low = low if len(low) == 1 else c
        else:
            u = low = c
        if u != c:
            want[("u", cp)] = ord(u)
        if low != c:
            want[("l", cp)] = ord(low)
    code, out, err = run(d, CASE, ["--max-memory", "64"])
    got = {}
    for line in out.splitlines():
        k, cp, to = line.split()
        got[(k, int(cp))] = int(to)
    diff = sorted(set(want.items()) ^ set(got.items()))[:5]
    case(f"case/upper and lower of every code point ({len(want)} mappings, identity elsewhere) (SC-003)", code == 0 and got == want,
         f"exit {code}, {len(got)} mappings, first differences {diff} {err[:200]}")
    table = open(os.path.join(ROOT, "src", "case_table.h")).read()
    case("case/src/case_table.h is what tools/gen_case.py writes", table == gen_case.header(), "regenerate with tools/gen_case.py --write")


def group_exponents(d):
    rnd = random.Random(42)
    lits = []
    while len(lits) < 10000:
        digits = "".join(rnd.choice("0123456789") for _ in range(rnd.randint(1, 17))).lstrip("0") or "0"
        cut = rnd.randint(0, len(digits))
        mant = digits if cut in (0, len(digits)) or rnd.random() < 0.3 else digits[:cut] + "." + digits[cut:]
        exp = rnd.choice([rnd.randint(-330, 300), rnd.randint(-30, 30), rnd.randint(-5, 5)])
        text = mant + rnd.choice("eE") + rnd.choice(["", "+", "-"] if exp >= 0 else ["-"]) + str(abs(exp))
        try:
            value = float(text)
        except ValueError:
            continue
        if value == float("inf"):
            continue
        lits.append(text)
    # str(x) is the shortest text that reads back as x, so float(str(x)) == float(literal) is exact
    parts = []
    for i in range(0, len(lits), 400):
        body = "".join(f"    print(str({t}));\n" for t in lits[i:i + 400])
        parts.append(f"@allow(\"W0703\", \"a table of literals\")\nfn part{i // 400}() uses console {{\n{body}}}\n")
    src = "\n".join(parts) + "\nfn main() uses console {\n" + "".join(f"    part{i}();\n" for i in range(len(parts))) + "}\n"
    code, out, err = run(d, src)
    got = out.splitlines()
    bad = [(t, g) for t, g in zip(lits, got) if float(g) != float(t)]
    case("exponents/10,000 literals have the value Python's float() gives (SC-004)", code == 0 and len(got) == len(lits) and not bad,
         f"exit {code}, {len(got)} lines, first differences {bad[:3]} {err[:300]}")


def text(rnd, n):
    alphabets = ["abcdefghij klmnop", "åäöéüñçß", "αβγδεζηθ", "привет", "日本語中文", "😀🎉🚀", "\t"]
    return "".join(rnd.choice(rnd.choice(alphabets)) for _ in range(n))


def group_loop(d):
    rnd = random.Random(3)
    with open(os.path.join(d, "texts.txt"), "w", encoding="utf-8") as f:
        for _ in range(200):
            f.write(text(rnd, rnd.randint(0, 300)) + "\n")
    code, out, err = run(d, LOOP)
    case("loop/for c in s visits what chars(s) lists, on 200 random texts", (code, out.strip()) == (0, "200 0"), f"{code} {out} {err[:300]}")
    block = text(rnd, 40000)
    size = (10 << 20) if SLOW == 1 else (1 << 20)
    with open(os.path.join(d, "big.txt"), "w", encoding="utf-8") as f:
        written = 0
        while written < size:
            f.write(block)
            written += len(block.encode())
    data = open(os.path.join(d, "big.txt"), encoding="utf-8").read()
    want = f"{len(data)} {sum(1 for c in data if ord(c) > 127)}"
    # (read_file itself holds the text twice while it checks it: 20 MiB for 10; the loop adds nothing)
    code, out, err = run(d, BIG.replace("CHARS", "s"), ["--max-memory", "32"])
    direct = out.split()
    case(f"loop/{size >> 20} MiB of text under a 32 MiB limit (SC-002)", code == 0 and " ".join(direct[:2]) == want, f"{code} {out} {err[:300]}")
    code, out, err = run(d, BIG.replace("CHARS", "chars(s)"), ["--max-memory", "2048"])
    listed = out.split()
    if code == 0 and len(direct) == 3 and len(listed) == 3:
        ratio = int(listed[2]) / max(1, int(direct[2]))
        print(f"strings: for c in s: {direct[2]} ms; for c in chars(s): {listed[2]} ms; {ratio:.1f} times faster (target 3)")
        if SLOW == 1:
            case("loop/faster than looping over chars(s) (SC-002)", ratio > 1.0, f"{ratio:.2f}")
    else:
        case("loop/the chars(s) comparison ran", False, f"{code} {out} {err[:300]}")


GROUPS = {"case": group_case, "exponents": group_exponents, "loop": group_loop}


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
    print(f"strings: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
