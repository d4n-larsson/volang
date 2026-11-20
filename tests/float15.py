"""Float text as vo wrote it before `str` gave the shortest text that reads back (15 significant
digits, plain decimal): for comparing today's output with a binary frozen before that change (the
oracle of diff_engines.py, the assembly release of diff_asm.py)."""
import re

TOKEN = re.compile(rb"(?<![\w.])-?\d+\.\d+(?![\w.])")


def _old(m):
    tok = m.group(0).decode()
    x = float(tok)
    if x == 0 or x != x or x in (float("inf"), float("-inf")):
        return m.group(0)
    mant, exp = ("%.14e" % x).split("e")  # exactly rounded, ties to even
    sign = "-" if mant.startswith("-") else ""
    digits = mant.lstrip("-").replace(".", "")
    e = int(exp)
    if e >= 0:
        whole, frac = (digits[:e + 1] + "0" * (e + 1 - len(digits))), digits[e + 1:]
    else:
        whole, frac = "0", "0" * (-e - 1) + digits
    frac = frac.rstrip("0") or "0"
    return (sign + whole + "." + frac).encode()


def old_text(data):
    """data (bytes) with every float written to 15 significant digits."""
    return TOKEN.sub(_old, data)


def same(a, b):
    """Two (stdout, stderr, status) results agree, floats compared at 15 significant digits."""
    return a == b or (old_text(a[0]), old_text(a[1]), a[2]) == (old_text(b[0]), old_text(b[1]), b[2])


if __name__ == "__main__":
    for new, old in ((b"4.800000000000001 3", b"4.8 3"), (b"0.30000000000000004", b"0.3"), (b"x 2.5 -0.0 1.0", b"x 2.5 -0.0 1.0"),
                     (b"0.3333333333333333", b"0.333333333333333"), (b"123456789012345.67", b"123456789012346.0"),
                     (b"-0.000012345678901234568", b"-0.0000123456789012346"), (b"p.vo:3:15 1.2.3", b"p.vo:3:15 1.2.3")):
        assert old_text(new) == old, (new, old_text(new), old)
    print("float15: ok")
