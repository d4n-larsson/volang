#!/usr/bin/env python3
"""Spec 043: std.bits and based literals against Python.

  bits.py                 every group (part of `make test`)
  bits.py --group NAME    one group: ops, algorithms, literals, speed

ops         10,000 pairs through the twelve operations, against Python with 64-bit masking (SC-001)
algorithms  tests/bits/{crc32,fnv1a,varint,sha256}.vo on 1,000 random inputs each, against zlib, hashlib
            and Python decoders (SC-002)
literals    10,000 random hex and binary literals of every length (SC-004)
speed       CRC-32 and FNV-1a over 1 MiB, each under 0.5 s on x86-64 (SC-003)
"""
import hashlib
import os
import random
import shutil
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
M = (1 << 64) - 1

failures = []
cases = 0

OPS = """import std.bits;
import std.fs;

fn row(a: int, b: int, n: int) -> list[int] {
    return [
        bits.bit_and(a, b),
        bits.bit_or(a, b),
        bits.bit_xor(a, b),
        bits.bit_not(a),
        bits.shift_left(a, n),
        bits.shift_right(a, n),
        bits.shift_right_logical(a, n),
        bits.rotate_left(a, n),
        bits.rotate_right(a, n),
        bits.wrapping_add(a, b),
        bits.wrapping_sub(a, b),
        bits.wrapping_mul(a, b)
    ];
}

fn text(n: int) -> str {
    return str(n);
}

fn main() -> result[void] uses console, file.read("./") {
    try fs.each_line("pairs.txt", fn(line: str) -> result[bool] {
        let p = split(line, " ");
        print(join(map(row(try parse_int(p[0]), try parse_int(p[1]), try parse_int(p[2])), text), " "));
        return ok(true);
    });
    return ok();
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def signed(u):
    u &= M
    return u - (1 << 64) if u >> 63 else u


def run(d, name, args=(), timeout=300):
    r = subprocess.run([VO, "run", "--timeout", str(120 * SLOW), name, "--"] + list(args), cwd=d, capture_output=True, text=True,
                       timeout=timeout * SLOW)
    return r.returncode, r.stdout, r.stderr


def group_ops(d):
    rnd = random.Random(43)
    special = [0, 1, -1, (1 << 63) - 1, -(1 << 63)] + [1 << k for k in range(63)] + [-(1 << k) for k in range(63)]

    def value():
        return rnd.choice(special) if rnd.random() < 0.4 else signed(rnd.getrandbits(64))

    rows = [(value(), value(), rnd.choice([0, 1, 31, 32, 63, rnd.randrange(64)])) for _ in range(10000)]
    with open(os.path.join(d, "pairs.txt"), "w") as f:
        f.write("".join(f"{a} {b} {n}\n" for a, b, n in rows))
    with open(os.path.join(d, "ops.vo"), "w") as f:
        f.write(OPS)
    want = []
    for a, b, n in rows:
        u = a & M
        want.append(" ".join(str(signed(x)) for x in (
            a & b, a | b, a ^ b, ~a, u << n, a >> n, u >> n, u << n | u >> (64 - n), u >> n | u << (64 - n), a + b, a - b, a * b)))
    code, out, err = run(d, "ops.vo")
    got = out.splitlines()
    bad = [(rows[i], w, g) for i, (w, g) in enumerate(zip(want, got)) if w != g][:2]
    case("ops/10,000 pairs through the twelve operations (SC-001)", code == 0 and got == want, f"exit {code}, {len(got)} lines, {bad} {err[:300]}")


def leb128(v):
    out = bytearray()
    while True:
        out.append(v & 0x7F | (0x80 if v >> 7 else 0))
        v >>= 7
        if not v:
            return bytes(out)


def fnv1a(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = (h ^ b) * 0x100000001B3 & M
    return signed(h)


def group_algorithms(d):
    rnd = random.Random(44)
    inputs = [rnd.randbytes(rnd.choice([0, 1, 55, 56, 63, 64, 65, rnd.randrange(300)])) for _ in range(1000)]
    numbers = [[rnd.getrandbits(rnd.choice([1, 7, 8, 32, 63, 64])) for _ in range(rnd.randrange(1, 8))] for _ in range(1000)]
    plans = [
        ("crc32", inputs, lambda b: str(zlib.crc32(b))),
        ("fnv1a", inputs, lambda b: str(fnv1a(b))),
        ("sha256", inputs, lambda b: hashlib.sha256(b).hexdigest()),
    ]
    for name, data, want in plans:
        with open(os.path.join(d, "inputs.txt"), "w") as f:
            f.write("".join(b.hex() + "\n" for b in data))
        shutil.copy(os.path.join(ROOT, "tests", "bits", name + ".vo"), d)
        code, out, err = run(d, name + ".vo")
        case(f"algorithms/{name} of 1,000 inputs (SC-002)", code == 0 and out.splitlines() == [want(b) for b in data],
             f"exit {code}, {len(out.splitlines())} lines, first {out[:80]!r} {err[:300]}")
    with open(os.path.join(d, "inputs.txt"), "w") as f:
        f.write("".join(b"".join(leb128(v) for v in vs).hex() + "\n" for vs in numbers))
    shutil.copy(os.path.join(ROOT, "tests", "bits", "varint.vo"), d)
    code, out, err = run(d, "varint.vo")
    case("algorithms/varint: 1,000 LEB128 sequences (SC-002)", code == 0 and out.splitlines() == [" ".join(str(signed(v)) for v in vs) for vs in numbers],
         f"exit {code}, first {out[:80]!r} {err[:300]}")


def group_literals(d):
    rnd = random.Random(45)
    lits = []
    for i in range(10000):
        if i % 2:
            n = i // 2 % 16 + 1
            lits.append("0x" + "".join(rnd.choice("0123456789abcdefABCDEF") for _ in range(n)))
        else:
            n = i // 2 % 64 + 1
            lits.append("0b" + "".join(rnd.choice("01") for _ in range(n)))
    parts = []
    for i in range(0, len(lits), 400):
        body = "".join(f"    print(str({t}));\n" for t in lits[i:i + 400])
        parts.append(f"@allow(\"W0703\", \"a table of literals\")\nfn part{i // 400}() uses console {{\n{body}}}\n")
    src = "\n".join(parts) + "\nfn main() uses console {\n" + "".join(f"    part{i}();\n" for i in range(len(parts))) + "}\n"
    with open(os.path.join(d, "lits.vo"), "w") as f:
        f.write(src)
    code, out, err = run(d, "lits.vo")
    got = out.splitlines()
    bad = [(t, g) for t, g in zip(lits, got) if str(signed(int(t, 0))) != g][:3]
    case("literals/10,000 hex and binary literals have Python's value as a 64-bit pattern (SC-004)", code == 0 and len(got) == len(lits) and not bad,
         f"exit {code}, {len(got)} lines, {bad} {err[:300]}")


def group_speed(d):
    data = random.Random(46).randbytes(1 << 20)
    with open(os.path.join(d, "big.bin"), "wb") as f:
        f.write(data)
    for name, want in (("crc32", zlib.crc32(data)), ("fnv1a", fnv1a(data))):
        shutil.copy(os.path.join(ROOT, "tests", "bits", name + ".vo"), d)
        best = None
        for _ in range(3):
            code, out, err = run(d, name + ".vo", ["whole"])
            parts = out.split()
            if code != 0 or len(parts) != 2 or parts[0] != str(want):
                case(f"speed/{name} over 1 MiB is right", False, f"exit {code} {out[:80]!r} {err[:300]}")
                break
            best = int(parts[1]) if best is None else min(best, int(parts[1]))
        else:
            print(f"bits: {name} over 1 MiB: {best} ms (bound 500 on x86-64)")
            if SLOW == 1 and not os.environ.get("CI"):
                case(f"speed/{name} over 1 MiB under 0.5 s (SC-003)", best < 500, f"{best} ms")


GROUPS = {"ops": group_ops, "algorithms": group_algorithms, "literals": group_literals, "speed": group_speed}


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
    print(f"bits: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
