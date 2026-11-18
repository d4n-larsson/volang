#!/usr/bin/env python3
"""Reference vectors for std.math (spec 020 research R7). Dev only: run through `make math-vectors`.

Writes tests/math/vectors.txt.gz: one call per line, `FN X [Y] EXPECTED`, every value in Python's
float.hex form ('nan', 'inf', '-0x0.0p+0', '0x1.8p+1'). Expected results are the correctly rounded
(nearest, ties to even) doubles of the exact values, computed with mpmath at 200 bits; a value within
2^-90 of a quantum of a rounding midpoint is recomputed at 1,000 bits
(and trusted to 2^-890). Rows whose call is R0007 in vo
(out of domain or a pole) are left out. Inputs: seeded random values over each domain, the first
inputs of CORE-MATH's published worst cases (<fn>.wc at the vendored commit), and special values.
"""
import gzip, math, os, random, re, resource, signal, sys, urllib.request

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "build", "py"))
import mpmath  # noqa: E402
from mpmath import mp, mpf  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..")
COMMIT = re.search(r"\| Commit \| `([0-9a-f]{40})`",
                   open(os.path.join(ROOT, "third_party/core-math/VENDOR.md")).read()).group(1)
WC_URL = "https://gitlab.inria.fr/core-math/core-math/-/raw/%s/src/binary64/%s/%s.wc"
RANDOM_N = 9000
WORST_N = 1000
FNS = ["sqrt", "exp", "log", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "pow"]
BIVARIATE = {"atan2", "pow"}

MP = {
    "sqrt": mpmath.sqrt, "exp": mpmath.exp, "log": mpmath.log, "sin": mpmath.sin, "cos": mpmath.cos,
    "tan": mpmath.tan, "asin": mpmath.asin, "acos": mpmath.acos, "atan": mpmath.atan,
    "atan2": mpmath.atan2, "pow": lambda x, y: mpmath.power(x, y),
}
PY = {
    "sqrt": math.sqrt, "exp": math.exp, "log": math.log, "sin": math.sin, "cos": math.cos,
    "tan": math.tan, "asin": math.asin, "acos": math.acos, "atan": math.atan, "atan2": math.atan2,
    "pow": math.pow,
}


def out_of_domain(fn, x, y=None):
    """True when vo stops with R0007 (research R5): NaN from non-NaN arguments, or a pole."""
    if math.isnan(x) or (y is not None and math.isnan(y)):
        return False
    if fn == "sqrt":
        return x < 0
    if fn == "log":
        return x <= 0
    if fn in ("sin", "cos", "tan"):
        return math.isinf(x)
    if fn in ("asin", "acos"):
        return abs(x) > 1
    if fn == "pow":
        if x == 0 and y < 0:
            return True
        return x < 0 and math.isfinite(y) and y != math.floor(y)
    return False


def round_double(v, prec=200):
    """The double nearest to the mpf v (ties to even), and whether v (computed at prec bits) was too
    close to a midpoint to trust: within 2^-(prec-110) of a quantum."""
    if v == 0:
        return 0.0, False
    sign, man, exp, bc = v._mpf_
    e = exp + bc - 1  # floor(log2 |v|)
    if e > 1023:
        return (-math.inf if sign else math.inf), False
    if e < -1076:  # below half the smallest subnormal: rounds to zero (and a mask this wide would
        return (-0.0 if sign else 0.0), False  # need gigabytes)
    qe = max(e, -1022) - 52  # exponent of one unit in the last place
    shift = exp - qe
    close = False
    if shift >= 0:
        n = man << shift
    else:
        n = man >> -shift
        r = man & ((1 << -shift) - 1)
        half = 1 << (-shift - 1)
        if abs(r - half) << (prec - 110) < (1 << -shift) and r != half:
            close = True
        if r > half or (r == half and n & 1):
            n += 1
    if n.bit_length() + qe > 1024:  # rounded up past the largest double
        return (-math.inf if sign else math.inf), False
    d = math.ldexp(n, qe)
    return (-d if sign else d), close


def expected(fn, x, y=None):
    args = (x,) if y is None else (x, y)
    if any(a == 0 or not math.isfinite(a) for a in args):
        # Special operands: IEEE 754 / C Annex F values, exact by definition; glibc gives them.
        try:
            return PY[fn](*args)
        except OverflowError:
            raise
    for prec in (200, 1000):
        mp.prec = prec
        if fn == "pow" and abs(y) > 2**20:
            # mpmath raises to the integer part of a large exponent exactly, with numbers that grow
            # without bound (it took the machine down once); exp(y log|x|) at the same precision has
            # the same correctly rounded result (these overflow or underflow, or x is near 1).
            v = mpmath.exp(mpf(y) * mpmath.log(abs(mpf(x))))
            if x < 0 and int(y) % 2:
                v = -v
        else:
            v = MP[fn](*[mpf(a) for a in args])
        if isinstance(v, mpmath.mpc):
            raise ValueError("complex result")
        d, close = round_double(v, prec)
        if not close:
            return d
    raise Slow()  # within 2^-900 of a midpoint even at 1,000 bits: leave the row out


def rand_double(rng, lo_exp, hi_exp, signed=True):
    e = rng.randint(lo_exp, hi_exp)
    m = rng.getrandbits(52)
    if e < -1022:  # subnormal
        d = math.ldexp(rng.getrandbits(52) or 1, -1074) if e == -1074 else math.ldexp(1 + m / 2**52, e)
    else:
        d = math.ldexp(1 + m / 2**52, e)
    return -d if signed and rng.random() < 0.5 else d


def random_inputs(fn, rng):
    out = []
    for i in range(RANDOM_N):
        if fn == "sqrt":
            out.append((rand_double(rng, -1074, 1023, signed=False),))
        elif fn == "log":
            out.append((rand_double(rng, -1074, 1023, signed=False) if i % 3 else 1 + rand_double(rng, -60, -1),))
        elif fn == "exp":
            out.append((rng.uniform(-745.2, 709.8) if i % 2 else rand_double(rng, -60, 9),))
        elif fn in ("sin", "cos", "tan"):
            if i % 4 == 0:
                k = rng.randint(1, 10**6)  # the double nearest to k*pi/2
                mp.prec = 200
                out.append((float(mpf(k) * mpmath.pi / 2),))
            elif i % 4 == 1:
                out.append((rand_double(rng, 30, 1023),))
            else:
                out.append((rand_double(rng, -40, 30),))
        elif fn in ("asin", "acos"):
            out.append((rng.uniform(-1, 1) if i % 3 else rand_double(rng, -60, -1),))
        elif fn == "atan":
            out.append((rand_double(rng, -80, 80) if i % 2 else rand_double(rng, -1074, 1023),))
        elif fn == "atan2":
            out.append((rand_double(rng, -200, 200), rand_double(rng, -200, 200)) if i % 2
                       else (rand_double(rng, -1074, 1023), rand_double(rng, -1074, 1023)))
        elif fn == "pow":
            if i % 3 == 0:  # negative base, integer exponent
                x = -rand_double(rng, -8, 8, signed=False)
                out.append((x, float(rng.randint(-60, 60))))
            elif i % 3 == 1:  # base near 1, large exponent
                x = 1 + rand_double(rng, -50, -3)
                out.append((x, rand_double(rng, 0, 40)))
            else:
                x = rand_double(rng, -100, 100, signed=False)
                lim = 1000 / max(abs(math.log2(x)), 1e-9)
                out.append((x, rng.uniform(-lim, lim)))
    return out


def worst_inputs(fn):
    rows = []
    if fn == "sqrt":  # hardware, not CORE-MATH: no worst-case file
        return rows
    with urllib.request.urlopen(WC_URL % (COMMIT, fn, fn)) as f:
        for line in f:
            line = line.decode().split("#")[0].strip()
            if not line:
                continue
            try:
                rows.append(tuple(float.fromhex(p.strip()) for p in line.split(",")))
            except ValueError:  # signaling NaNs and other values vo cannot make
                continue
            if len(rows) >= WORST_N * 2:
                break
    return rows


SPECIAL = [0.0, -0.0, math.inf, -math.inf, math.nan, 5e-324, -5e-324, 1.7976931348623157e308,
           -1.7976931348623157e308, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0]


class Slow(Exception):
    pass


def on_alarm(signum, frame):
    raise Slow()


def main():
    # mpmath can try to compute some pow inputs exactly and use unbounded memory (it took the machine
    # down once): cap this process at 4 GiB and each call at 20 s, and leave such rows out, logged.
    resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
    signal.signal(signal.SIGALRM, on_alarm)
    rng = random.Random(20201118)
    path = os.path.join(ROOT, "tests/math/vectors.txt.gz")
    total = 0
    with gzip.open(path + ".tmp", "wt", compresslevel=9) as out:
        out.write("# std.math reference vectors (spec 020): FN X [Y] EXPECTED, float.hex; made by "
                  "tools/math_vectors_gen.py\n")
        for fn in FNS:
            groups = [("random", random_inputs(fn, rng)), ("worst", worst_inputs(fn))]
            if fn in BIVARIATE:
                groups.append(("special", [(a, b) for a in SPECIAL for b in SPECIAL]))
            else:
                groups.append(("special", [(a,) for a in SPECIAL]))
            for tag, inputs in groups:
                out.write("# %s %s\n" % (fn, tag))
                n = 0
                for args in inputs:
                    if tag == "worst" and n >= WORST_N:
                        break
                    if out_of_domain(fn, *args):
                        continue
                    try:
                        signal.alarm(20)
                        r = expected(fn, *args)
                    except (ValueError, OverflowError):
                        continue
                    except (MemoryError, Slow):
                        print("skipped (too costly): %s %s" % (fn, " ".join(float.hex(a) for a in args)),
                              file=sys.stderr)
                        continue
                    finally:
                        signal.alarm(0)
                    out.write("%s %s %s\n" % (fn, " ".join(float.hex(a) for a in args), float.hex(r)))
                    n += 1
                total += n
                print("%-6s %-8s %d" % (fn, tag, n), file=sys.stderr)
    os.replace(path + ".tmp", path)
    print("wrote %d rows to %s" % (total, path), file=sys.stderr)


if __name__ == "__main__":
    main()
