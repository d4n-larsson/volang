#!/usr/bin/env python3
"""Spec 035 SC-001–SC-003: std.crypto against Python's hashlib and hmac.

- 200 random inputs (0 to 300 bytes, keys 0 to 200 bytes) hashed and signed by one vo program,
  compared with hashlib.sha256 and hmac;
- 5 stored passwords at costs 1 to 2,000 built with hashlib.pbkdf2_hmac: verify_password accepts the
  right password and refuses another;
- SC-002: 1,000 tokens of 16 bytes from one run are all different, and a second run shares none.

  crypto_vectors.py            the checks (part of `make test`)
  crypto_vectors.py --time     SC-003: one hash_password between 50 ms and 1 s (timing-sensitive; the
                               figure is this machine's)
"""
import base64
import hashlib
import hmac
import os
import random
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN (default build/vo) natively or under qemu

IMPORTS = """import std.bytes.from_hex;
import std.bytes.from_text;
import std.bytes.to_hex;
import std.crypto.hash_password;
import std.crypto.hmac_sha256;
import std.crypto.random_bytes;
import std.crypto.sha256;
import std.crypto.verify_password;
"""


def run(body, uses="console", timeout=300):
    src = IMPORTS + f"\nfn main() -> result[void] uses {uses} {{\n{body}\n    return ok();\n}}\n"
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "m.vo"), "w").write(src)
        t = time.perf_counter()
        r = subprocess.run([VO, "run", "--timeout", "0", "m.vo"], cwd=d, capture_output=True, text=True, timeout=timeout)
        took = time.perf_counter() - t
        if r.returncode:
            raise SystemExit(f"vo run fails:\n{r.stdout}{r.stderr}")
        return r.stdout.splitlines(), took


def timing():
    one = 'let s = hash_password("correct horse battery");\n    print(str(len(s)));'
    base = statistics.median(run('print("x");')[1] for _ in range(5))
    took = statistics.median(run(one, "console, entropy")[1] for _ in range(5)) - base
    ok = 0.05 <= took <= 1.0
    print(f"SC-003 one hash_password: {took * 1000:.0f} ms on this machine (limits 50 ms to 1 s)")
    return ok


def main():
    if "--time" in sys.argv:
        sys.exit(0 if timing() else 1)
    fails = []
    rng = random.Random(35)
    cases = [(rng.randbytes(rng.randrange(0, 201)), rng.randbytes(rng.randrange(0, 301))) for _ in range(200)]
    cases += [(b"", b""), (b"k" * 64, b"m" * 64), (b"k" * 65, b"m" * 55), (b"k" * 63, b"m" * 56), (b"", b"m" * 119)]
    lines = []
    for key, msg in cases:
        lines.append(f'    print(to_hex(sha256(try from_hex("{msg.hex()}"))) + " " + '
                     f'to_hex(hmac_sha256(try from_hex("{key.hex()}"), try from_hex("{msg.hex()}"))));')
    out, _ = run("\n".join(lines))
    for (key, msg), got in zip(cases, out):
        want = hashlib.sha256(msg).hexdigest() + " " + hmac.new(key, msg, "sha256").hexdigest()
        if got != want:
            fails.append(f"sha256/hmac of {len(msg)} bytes under a {len(key)}-byte key: {got} != {want}")
    if len(out) != len(cases):
        fails.append(f"{len(out)} lines for {len(cases)} cases")

    pw = []
    for cost in (1, 2, 3, 1000, 2000):
        password = "pässword %d" % cost
        salt = rng.randbytes(rng.choice([0, 1, 16, 40]))
        key = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, cost, 32)
        stored = f"pbkdf2-sha256${cost}${base64.b64encode(salt).decode()}${base64.b64encode(key).decode()}"
        pw.append(f'    print(str(verify_password("{password}", "{stored}")) + " " + '
                  f'str(verify_password("{password}x", "{stored}")));')
    out, _ = run("\n".join(pw))
    if out != ["true false"] * 5:
        fails.append(f"verify_password against hashlib.pbkdf2_hmac: {out}")

    tokens = "    for i in 0..1000 {\n        print(to_hex(random_bytes(16)));\n    }"
    a, _ = run(tokens, "console, entropy")
    b, _ = run(tokens, "console, entropy")
    if len(set(a)) != 1000 or any(len(t) != 32 for t in a):
        fails.append("SC-002: the 1,000 tokens of one run are not all different 16-byte values")
    if set(a) & set(b):
        fails.append("SC-002: two runs share a token")
    for f in fails:
        print("FAIL", f)
    print(f"crypto_vectors: {len(cases)} hashes and signatures, 5 stored passwords, 2 x 1000 tokens, "
          f"{len(fails)} failures")
    sys.exit(1 if fails else 0)


main()
