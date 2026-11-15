#!/usr/bin/env python3
"""Spec 007 SC-004 and T019: package content pins.

1. Tampering: from a clean, pinned copy of a small project (a package with a nested module and a
   non-.vo file), every changed byte, added, removed or renamed .vo file must stop `vo run` with
   E0701 before the program runs; changing or adding a non-.vo file must not matter.
2. SHA-256: `vo package` prints the package hash; for single-file packages of every content length
   0..130 bytes (every padding case of the 64-byte block) and 1,000,000 x "a", it must equal the
   hash Python's hashlib computes over the same framing (contracts/packages.md: for each .vo file in
   byte order of relative paths, PATH NUL LENGTH NUL BYTES).
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN natively or under qemu

MAIN = """import money.format.cents;
import money.util.round.half_up;

fn main() uses console {
    print(cents(half_up(1234)));
}
"""
FORMAT = """# Formats amounts of money.
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
"""
ROUND = """export fn half_up(n: int) -> int {
    return n;
}
"""


def vo(args, cwd):
    r = subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=120)
    return r.returncode, (r.stdout + r.stderr).decode(errors="replace")


def project(base):
    d = os.path.join(base, "p")
    os.makedirs(os.path.join(d, "shared", "money", "util"))
    files = {"main.vo": MAIN, "shared/money/format.vo": FORMAT, "shared/money/util/round.vo": ROUND,
             "shared/money/README.txt": "notes\n"}
    for rel, text in files.items():
        open(os.path.join(d, rel), "w").write(text)
    code, out = vo(["package", "main.vo", "money", "shared/money"], d)
    assert code == 0, out
    code, out = vo(["run", "main.vo"], d)
    assert code == 0 and out == "12.34\n", out
    return d


def edit(path, fn):
    data = open(path, "rb").read()
    open(path, "wb").write(fn(data))


def tampers(m):
    """(name, change) pairs; m is the package directory. Every change must be caught."""
    fmt, rnd, util = os.path.join(m, "format.vo"), os.path.join(m, "util", "round.vo"), os.path.join(m, "util")

    def flip(at):
        return lambda d: d[:at(d)] + bytes([d[at(d)] ^ 0x20]) + d[at(d) + 1:]
    return [
        ("format: first byte", lambda: edit(fmt, flip(lambda d: 0))),
        ("format: middle byte", lambda: edit(fmt, flip(lambda d: len(d) // 2))),
        ("format: last byte", lambda: edit(fmt, lambda d: d[:-1] + b" ")),
        ("format: one byte appended", lambda: edit(fmt, lambda d: d + b"\n")),
        ("format: last byte removed", lambda: edit(fmt, lambda d: d[:-1])),
        ("format: two bytes swapped", lambda: edit(fmt, lambda d: d[:3] + d[4:5] + d[3:4] + d[5:])),
        ("format: same length, other text", lambda: edit(fmt, lambda d: d.replace(b"Formats", b"FORMATS"))),
        ("round (nested): first byte", lambda: edit(rnd, flip(lambda d: 0))),
        ("round (nested): middle byte", lambda: edit(rnd, flip(lambda d: len(d) // 2))),
        ("round (nested): last byte", lambda: edit(rnd, lambda d: d[:-1] + b" ")),
        ("round (nested): emptied", lambda: edit(rnd, lambda d: b"")),
        ("added: top-level .vo", lambda: open(os.path.join(m, "extra.vo"), "w").write("fn x() -> int {\n    return 1;\n}\n")),
        ("added: empty .vo", lambda: open(os.path.join(m, "empty.vo"), "w").close()),
        ("added: nested .vo", lambda: open(os.path.join(util, "more.vo"), "w").write("fn y() -> int {\n    return 2;\n}\n")),
        ("added: .vo in a new directory", lambda: (os.makedirs(os.path.join(m, "new")),
                                                  open(os.path.join(m, "new", "z.vo"), "w").write("fn z() -> int {\n    return 3;\n}\n"))),
        ("removed: nested .vo", lambda: os.remove(rnd)),
        ("removed: top-level .vo", lambda: os.remove(fmt)),
        ("renamed: nested file", lambda: os.rename(rnd, os.path.join(util, "round2.vo"))),
        ("renamed: top-level file", lambda: os.rename(fmt, os.path.join(m, "formats.vo"))),
        ("renamed: directory", lambda: os.rename(util, os.path.join(m, "utils"))),
        ("moved: file into another directory", lambda: os.rename(fmt, os.path.join(util, "format.vo"))),
        ("renamed: .vo to another extension", lambda: os.rename(rnd, os.path.join(util, "round.txt"))),
    ]


def harmless(m):
    """Changes outside the hash: the program must still run."""
    return [
        ("non-.vo file changed", lambda: edit(os.path.join(m, "README.txt"), lambda d: d + b"more\n")),
        ("non-.vo file added", lambda: open(os.path.join(m, "notes.md"), "w").write("x\n")),
    ]


def framed(files):
    h = hashlib.sha256()
    for rel in sorted(files, key=lambda r: r.encode()):
        data = files[rel]
        h.update(rel.encode() + b"\0" + str(len(data)).encode() + b"\0" + data)
    return "sha256:" + h.hexdigest()


def main():
    bad = 0
    base = tempfile.mkdtemp(prefix="vo-packages-")
    try:
        clean = project(base)
        cases = [(n, f, True) for n, f in tampers(os.path.join(clean, "shared", "money"))]
        cases += [(n, f, False) for n, f in harmless(os.path.join(clean, "shared", "money"))]
        for i, (name, _, caught) in enumerate(cases):
            d = os.path.join(base, f"t{i}")
            shutil.copytree(clean, d)
            m = os.path.join(d, "shared", "money")
            change = (tampers(m) if caught else harmless(m))[i if caught else i - len(tampers(m))][1]
            change()
            code, out = vo(["run", "main.vo"], d)
            ok = (code == 65 and "E0701" in out and "12.34" not in out) if caught else (code == 0 and out == "12.34\n")
            if not ok:
                bad += 1
                print(f"FAIL {name}: exit {code}\n{out}")
        n_tamper = len(tampers(clean))
        print(f"packages_check: {n_tamper} tamper variants caught before running, "
              f"{len(cases) - n_tamper} harmless changes ignored" if not bad else f"packages_check: {bad} failed")

        hashes = 0
        lengths = list(range(0, 131)) + [1000000]
        for n in lengths:
            d = os.path.join(base, f"h{n}")
            os.makedirs(os.path.join(d, "pkg"))
            data = (b"a" * n) if n == 1000000 else bytes((i * 7 + 3) % 251 for i in range(n))
            open(os.path.join(d, "pkg", "a.vo"), "wb").write(data)
            open(os.path.join(d, "main.vo"), "w").write("")
            code, out = vo(["package", "main.vo", "h", "pkg"], d)
            got = re.search(r"sha256:[0-9a-f]{64}", out)
            want = framed({"a.vo": data})
            if code != 0 or not got or got.group(0) != want:
                bad += 1
                print(f"FAIL sha256 for {n} bytes: {out.strip()} (hashlib {want})")
            hashes += 1
        print(f"packages_check: {hashes} package hashes equal hashlib's (0..130 bytes and 1,000,000 x 'a')")
    finally:
        shutil.rmtree(base, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
