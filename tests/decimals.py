#!/usr/bin/env python3
"""Spec 044: std.decimal against Python's decimal module.

  decimals.py                 every group (part of `make test`)
  decimals.py --group NAME    one group: arith, round, text, alloc, speed

arith  add, sub, mul, compare, equal, normalize: 10,000 random cases each (SC-001)
round  round_to and div in the six modes: 10,000 each (SC-001)
text   parse_decimal, decimal_text and grouped_text (10,000 each), to_int, to_float (SC-001)
alloc  split_evenly and allocate: the parts sum to the amount, each within a unit of its share (SC-002)
speed  100,000 invoice lines under 1 s on x86-64 (SC-003)

A case whose exact result does not fit 64 bits of units or 18 places must stop with R0001: those run
one per process, at most 200 per group.
"""
import decimal as pydecimal
import os
import random
import re
import subprocess
import sys
import tempfile
from decimal import Decimal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
N = 10000 if SLOW == 1 else 1000
LO, HI = -(1 << 63), (1 << 63) - 1
LIMIT = 2147483647
MODES = {
    "half_even": pydecimal.ROUND_HALF_EVEN, "half_up": pydecimal.ROUND_HALF_UP, "down": pydecimal.ROUND_DOWN,
    "up": pydecimal.ROUND_UP, "floor": pydecimal.ROUND_FLOOR, "ceiling": pydecimal.ROUND_CEILING,
}
pydecimal.getcontext().prec = 80

failures = []
cases = 0

DRIVER = """import std.decimal;
import std.fs;

fn mode(s: str) -> decimal.Rounding {
    if s == "half_even" {
        return decimal.Rounding.half_even;
    }
    if s == "half_up" {
        return decimal.Rounding.half_up;
    }
    if s == "down" {
        return decimal.Rounding.down;
    }
    if s == "up" {
        return decimal.Rounding.up;
    }
    if s == "floor" {
        return decimal.Rounding.floor;
    }
    return decimal.Rounding.ceiling;
}

fn show(d: decimal.Decimal) -> str {
    return str(d.units) + " " + str(d.places);
}

fn parts(r: result[list[decimal.Decimal]]) -> str {
    match r {
        case ok(ds) {
            return join(map(ds, show), ",");
        }
        case err(_) {
            return "err";
        }
    }
}

fn ratios(s: str) -> result[list[int]] {
    let out = new list[int];
    if s != "-" {
        for piece in split(s, ",") {
            push(out, try parse_int(piece));
        }
    }
    return ok(out);
}

fn answer(p: list[str]) -> result[str] {
    let op = p[0];
    if op == "parse" {
        match decimal.parse_decimal(p[1]) {
            case ok(d) {
                return ok(show(d));
            }
            case err(_) {
                return ok("err");
            }
        }
    }
    let a = decimal.decimal(try parse_int(p[1]), try parse_int(p[2]));
    if op == "text" {
        return ok(decimal.decimal_text(a) + " " + decimal.grouped_text(a));
    }
    if op == "norm" {
        return ok(show(decimal.normalize(a)));
    }
    if op == "toint" {
        match decimal.to_int(a) {
            case ok(n) {
                return ok(str(n));
            }
            case err(_) {
                return ok("err");
            }
        }
    }
    if op == "tofloat" {
        return ok(str(decimal.to_float(a)));
    }
    if op == "round" {
        return ok(show(decimal.round_to(a, try parse_int(p[3]), mode(p[4]))));
    }
    if op == "split" {
        return ok(parts(decimal.split_evenly(a, try parse_int(p[3]))));
    }
    if op == "alloc" {
        return ok(parts(decimal.allocate(a, try ratios(p[3]))));
    }
    let b = decimal.decimal(try parse_int(p[3]), try parse_int(p[4]));
    if op == "add" {
        return ok(show(decimal.add(a, b)));
    }
    if op == "sub" {
        return ok(show(decimal.sub(a, b)));
    }
    if op == "mul" {
        return ok(show(decimal.mul(a, b)));
    }
    if op == "cmp" {
        return ok(str(decimal.compare(a, b)) + " " + str(decimal.equal(a, b)));
    }
    return ok(show(decimal.div(a, b, try parse_int(p[5]), mode(p[6]))));
}

fn main() -> result[void] uses console, file.read("./") {
    try fs.each_line("cases.txt", fn(line) {
        print(try answer(split(line, " ")));
        return ok(true);
    });
    return ok();
}
"""

INVOICE = """import std.decimal;

fn main() uses console, clock {
    let start = now_ms();
    var sum = decimal.decimal(0, 2);
    let vat = decimal.decimal(25, 2);
    for i in 0..100000 {
        let net = decimal.mul(decimal.decimal(1999 + i % 500, 2), decimal.from_int(1 + i % 7));
        let tax = decimal.round_to(decimal.mul(net, vat), 2, decimal.Rounding.half_even);
        set sum = decimal.add(sum, decimal.add(net, tax));
    }
    print(decimal.decimal_text(sum) + " " + str(now_ms() - start));
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def val(u, p):
    return Decimal(u).scaleb(-p)


def fits(u, p):
    return LO <= u <= HI and 0 <= p <= 18


def rand_dec(rnd):
    p = rnd.choice([0, 2, 2, 3, 18, rnd.randrange(19)])
    k = rnd.random()
    if k < 0.1:
        u = rnd.choice([0, 1, -1, LO, HI, LO + 1, 5, -5, 10 ** 18, -(10 ** 18), 922337203685477581, HI // 10])
    elif k < 0.5:
        u = rnd.randrange(-10 ** 7, 10 ** 7)
    else:
        u = rnd.getrandbits(rnd.randrange(1, 64)) * rnd.choice([1, -1])
    return u, p


def run_driver(d, lines, timeout=300):
    with open(os.path.join(d, "driver.vo"), "w") as f:
        f.write(DRIVER)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("".join(line + "\n" for line in lines))
    r = subprocess.run([VO, "run", "--timeout", str(120 * SLOW), "driver.vo"], cwd=d, capture_output=True, text=True,
                       timeout=timeout * SLOW)
    return r.returncode, r.stdout.splitlines(), r.stderr


def check(d, name, rows):
    """rows: (line, expected text) or (line, None) when the exact result does not fit."""
    plain = [(line, want) for line, want in rows if want is not None]
    code, got, err = run_driver(d, [line for line, _ in plain])
    bad = [(line, want, g) for (line, want), g in zip(plain, got) if want != g][:3]
    case(f"{name}: {len(plain)} cases as Python's decimal", code == 0 and len(got) == len(plain) and not bad,
         f"exit {code}, {len(got)} lines, {bad} {err[:300]}")
    stops = [line for line, want in rows if want is None][:200 if SLOW == 1 else 20]
    wrong = []
    for line in stops:
        code, got, err = run_driver(d, [line])
        if code == 0 or "R0001" not in err:
            wrong.append((line, got[:1], err[:80]))
    case(f"{name}: {len(stops)} results that do not fit stop with R0001", not wrong, str(wrong[:3]))


def show(u, p):
    return f"{u} {p}" if fits(u, p) else None


def group_arith(d):
    rnd = random.Random(441)
    for op in ("add", "sub", "mul"):
        rows = []
        for _ in range(N):
            (a, pa), (b, pb) = rand_dec(rnd), rand_dec(rnd)
            if op == "mul":
                rows.append((f"mul {a} {pa} {b} {pb}", show(a * b, pa + pb)))
            else:
                p = max(pa, pb)
                x, y = a * 10 ** (p - pa), b * 10 ** (p - pb)
                rows.append((f"{op} {a} {pa} {b} {pb}", show(x + y if op == "add" else x - y, p)))
        check(d, f"arith/{op}", rows)
    rows = []
    for _ in range(N):
        (a, pa), (b, pb) = rand_dec(rnd), rand_dec(rnd)
        if rnd.random() < 0.3:
            pb = rnd.randrange(19)
            b = a * 10 ** (pb - pa) if pb >= pa else a // 10 ** (pa - pb)
            if not fits(b, pb):
                continue
        x, y = val(a, pa), val(b, pb)
        rows.append((f"cmp {a} {pa} {b} {pb}", f"{(x > y) - (x < y)} {'true' if x == y else 'false'}"))
    check(d, "arith/compare and equal", rows)
    rows = []
    for _ in range(N):
        a, pa = rand_dec(rnd)
        if rnd.random() < 0.5:
            k = 10 ** rnd.randrange(pa + 1)
            a = a // k * k if fits(a // k * k, pa) else 0
        u, p = a, pa
        while p > 0 and u % 10 == 0:
            u, p = u // 10, p - 1
        rows.append((f"norm {a} {pa}", f"{u} {p}"))
    check(d, "arith/normalize", rows)


def quantized(x, places, mode):
    q = x.quantize(Decimal(1).scaleb(-places), rounding=MODES[mode])
    return show(int(q.scaleb(places)), places)


def group_round(d):
    rnd = random.Random(442)
    rows = []
    for _ in range(N):
        a, pa = rand_dec(rnd)
        if rnd.random() < 0.3 and pa > 0:  # exact ties
            a = a - a % 10 ** pa + 5 * 10 ** (pa - 1)
            if not fits(a, pa):
                continue
        places, mode = rnd.choice([0, 2, max(0, pa - 1), rnd.randrange(19)]), rnd.choice(list(MODES))
        rows.append((f"round {a} {pa} {places} {mode}", quantized(val(a, pa), places, mode)))
    check(d, "round/round_to", rows)
    rows = []
    while len(rows) < N:
        (a, pa), (b, pb) = rand_dec(rnd), rand_dec(rnd)
        if b == 0:
            continue
        if rnd.random() < 0.2:  # exact ties: a = b * (odd / 2) at some places
            b = rnd.choice([2, 4, 8, -2, 20, 200, 1 << 62, LO])
        places, mode = rnd.choice([0, 2, 2, 4, 18, rnd.randrange(19)]), rnd.choice(list(MODES))
        rows.append((f"div {a} {pa} {b} {pb} {places} {mode}", quantized(val(a, pa) / val(b, pb), places, mode)))
    check(d, "round/div", rows)


def rand_text(rnd):
    u, p = rand_dec(rnd)
    t = format(val(u, p), "f")
    k = rnd.random()
    if k < 0.55:
        return t
    if k < 0.65:
        return t.replace("-", "-00") if t.startswith("-") else "00" + t
    if k < 0.7:
        return ("-" if rnd.random() < 0.5 else "") + str(rnd.getrandbits(70)) + rnd.choice(["", ".5"])
    if k < 0.75:
        return "0." + "".join(rnd.choice("0123456789") for _ in range(rnd.randrange(17, 22)))
    i = rnd.randrange(len(t) + 1)
    return t[:i] + rnd.choice(["", ".", "-", "+", "e", "x", ",", "_", "1e3"]) + t[i + (rnd.random() < 0.5):]


def group_text(d):
    rnd = random.Random(443)
    rows = []
    for _ in range(N):
        t = rand_text(rnd)
        want = "err"
        if re.fullmatch(r"-?[0-9]+(\.[0-9]{1,18})?", t):
            p = len(t.partition(".")[2])
            u = int(t.replace(".", ""))
            if fits(u, p):
                want = f"{u} {p}"
        rows.append((f"parse {t}", want))
    check(d, "text/parse_decimal", rows)
    rows, ints, floats = [], [], []
    for _ in range(N):
        u, p = rand_dec(rnd)
        x = val(u, p)
        rows.append((f"text {u} {p}", f"{x:f} {x:,f}"))
        if len(ints) < N // 5:
            if rnd.random() < 0.5:
                u -= u % 10 ** p if u >= 0 else -(-u % 10 ** p)
                x = val(u, p)
            ints.append((f"toint {u} {p}", str(int(x)) if x == x.to_integral_value() else "err"))
            floats.append((f"tofloat {u} {p}", float(x)))
    check(d, "text/decimal_text and grouped_text", rows)
    check(d, "text/to_int", ints)
    code, got, err = run_driver(d, [line for line, _ in floats])
    bad = [(line, want, g) for (line, want), g in zip(floats, got) if float(g) != want][:3]
    case(f"text/to_float: {len(floats)} cases are the nearest float", code == 0 and len(got) == len(floats) and not bad,
         f"exit {code}, {bad} {err[:300]}")


def shares(u, ratios):
    total = sum(ratios)
    mag = abs(u)
    parts = [mag * r // total for r in ratios]
    left = mag - sum(parts)
    for i, r in enumerate(ratios):
        if left and r > 0:
            parts[i] += 1
            left -= 1
    return [x if u >= 0 else -x for x in parts]


def group_alloc(d):
    rnd = random.Random(444)
    rows = []
    for _ in range(N):
        u, p = rand_dec(rnd)
        if rnd.random() < 0.5:
            n = rnd.choice([0, -1, 1, 2, 3, 7, rnd.randrange(1, 40)])
            want = "err" if n < 1 else ",".join(f"{x} {p}" for x in shares(u, [1] * n))
            rows.append((f"split {u} {p} {n}", want))
        else:
            ratios = [rnd.choice([0, 1, 1, 2, 3, 7, rnd.randrange(1000), rnd.randrange(LIMIT // 8)]) for _ in range(rnd.randrange(7))]
            if rnd.random() < 0.05 and ratios:
                ratios[0] = rnd.choice([-1, LIMIT, LIMIT + 1])
            bad = not ratios or min(ratios) < 0 or sum(ratios) == 0 or sum(ratios) > LIMIT
            want = "err" if bad else ",".join(f"{x} {p}" for x in shares(u, ratios))
            rows.append((f"alloc {u} {p} {','.join(map(str, ratios)) or '-'}", want))
            if not bad:  # the property itself, on the expected parts
                got = shares(u, ratios)
                assert sum(got) == u and all(abs(Decimal(g) - Decimal(u) * r / sum(ratios)) <= 1 for g, r in zip(got, ratios))
    check(d, "alloc/split_evenly and allocate sum to the amount, each part within a unit of its share (SC-002)", rows)


def group_speed(d):
    total = Decimal(0)
    for i in range(100000):
        net = Decimal(1999 + i % 500).scaleb(-2) * (1 + i % 7)
        total += net + (net * Decimal("0.25")).quantize(Decimal("0.01"), rounding=pydecimal.ROUND_HALF_EVEN)
    with open(os.path.join(d, "invoice.vo"), "w") as f:
        f.write(INVOICE)
    best = None
    for _ in range(3):
        r = subprocess.run([VO, "run", "--timeout", str(120 * SLOW), "invoice.vo"], cwd=d, capture_output=True, text=True, timeout=300 * SLOW)
        parts = r.stdout.split()
        if r.returncode != 0 or len(parts) != 2 or parts[0] != f"{total:f}":
            case("speed/the invoice total is right", False, f"exit {r.returncode} {r.stdout[:80]!r} want {total:f} {r.stderr[:300]}")
            return
        best = int(parts[1]) if best is None else min(best, int(parts[1]))
    print(f"decimal: 100,000 invoice lines: {best} ms (bound 1000 on x86-64)")
    if SLOW == 1 and not os.environ.get("CI"):
        case("speed/100,000 invoice lines under 1 s (SC-003)", best < 1000, f"{best} ms")


GROUPS = {"arith": group_arith, "round": group_round, "text": group_text, "alloc": group_alloc, "speed": group_speed}


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
    print(f"decimal: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
