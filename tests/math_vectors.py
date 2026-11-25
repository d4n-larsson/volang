#!/usr/bin/env python3
"""Spec 020 (SC-001, SC-002, task T015): every row of tests/math/vectors.txt.gz through std.math, bit
for bit.

For each function, one vo program reads its rows' inputs on stdin and prints each result. Inputs go
in as Python's shortest round-trip decimal text, read by std.json (exact; `nan`, `inf` and `-inf` are
made by division); results come back through std.json.write_json (exact) or, when not finite, str.
Runs bin/vo, so VO_BIN selects the build (another ARCH runs under qemu). Exits 1 on any mismatch and
prints the first ten.
"""
import gzip, math, os, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
VECTORS = os.path.join(ROOT, "tests", "math", "vectors.txt.gz")

PROGRAM = """import std.math;
import std.json;

fn num(s: str) -> result[float] {{
    if s == "nan" {{
        return ok(0.0 / 0.0);
    }}
    if s == "inf" {{
        return ok(1.0 / 0.0);
    }}
    if s == "-inf" {{
        return ok(-1.0 / 0.0);
    }}
    let j = try json.parse_json(s);
    return json.as_float(j);
}}

fn show(r: float) -> str {{
    if r != r or r - r != 0.0 {{
        return str(r);
    }}
    match json.write_json(json.Json.number(r)) {{
        case ok(t) {{
            return t;
        }}
        case err(e) {{
            return "error: " + e;
        }}
    }}
}}

fn main() uses console, input {{
    for line in split(read_all(), "\\n") {{
        if len(line) == 0 {{
            continue;
        }}
        let p = split(line, " ");
        match num(p[0]) {{
            case ok(x) {{
                {call}
            }}
            case err(e) {{
                print("error: " + e);
            }}
        }}
    }}
}}
"""
CALL1 = "print(show(math.{fn}(x)));"
CALL2 = """match num(p[1]) {{
                    case ok(y) {{
                        print(show(math.{fn}(x, y)));
                    }}
                    case err(e) {{
                        print("error: " + e);
                    }}
                }}"""


def text(x):
    return "nan" if x != x else "inf" if x == math.inf else "-inf" if x == -math.inf else repr(x)


def bits(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def same(a, b):
    return (a != a and b != b) or bits(a) == bits(b)


def main():
    rows = {}
    with gzip.open(VECTORS, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue
            p = line.split()
            rows.setdefault(p[0], []).append([float.fromhex(v) for v in p[1:]])
    failures, total = [], 0
    with tempfile.TemporaryDirectory() as tmp:
        for fn, rs in rows.items():
            two = len(rs[0]) == 3
            src = PROGRAM.format(fn=fn, call=(CALL2 if two else CALL1).format(fn=fn))
            path = os.path.join(tmp, fn + ".vo")
            with open(path, "w") as f:
                f.write(src)
            stdin = "".join(" ".join(text(a) for a in r[:-1]) + "\n" for r in rs)
            p = subprocess.run([VO, "run", path], input=stdin, capture_output=True, text=True)
            out = p.stdout.splitlines()
            if p.returncode != 0 or len(out) != len(rs):
                failures.append(f"{fn}: exit {p.returncode}, {len(out)} results for {len(rs)} rows\n"
                                f"{p.stderr[:2000]}")
                continue
            bad = 0
            for r, got_text in zip(rs, out):
                try:
                    got = float(got_text)
                except ValueError:
                    got = None
                if got is None or not same(got, r[-1]):
                    bad += 1
                    failures.append(f"{fn}({', '.join(float.hex(a) for a in r[:-1])}): want "
                                    f"{float.hex(r[-1])}, got {got_text}")
            total += len(rs)
            print(f"math_vectors: {fn}: {len(rs)} rows, {bad} mismatches")
    for f in failures[:10]:
        print("FAIL", f)
    print(f"math_vectors: {total} rows, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
