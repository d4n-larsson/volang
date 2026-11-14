#!/usr/bin/env python3
"""Writes the std.json corpus (spec 007 T013): tests/json/{valid,invalid}/NNN.json + NNN.expected.

Expected values come from RFC 8259 and contracts/library.md, not from vo:
- valid: the document as `write_json` must print it (Python's json reads it; integers that fit
  in 64 bits stay integers, other numbers are written with the first of 15, 16 or 17 digits that
  reads back, in plain decimal; objects keep each key's first position and its last value);
- invalid: `line L, column C` of the first character that cannot continue a valid document
  (a misspelled word, a surrogate escape and an out-of-range number: their first character).
Run once; the files are checked in.
"""
import json
import os
import random
from decimal import Decimal

HERE = os.path.dirname(os.path.abspath(__file__))


def wfloat(f):
    """The first of 15, 16, 17 significant digits (correctly rounded) that reads back to f, in
    plain decimal with at least one digit after the point."""
    if f == 0:
        return "-0.0" if str(f).startswith("-") else "0.0"
    for n in (15, 16, 17):
        t = "%.*e" % (n - 1, f)
        if float(t) == f:
            break
    t = format(Decimal(t).normalize(), "f")
    return t if "." in t else t + ".0"


def wstr(s):
    out = []
    for c in s:
        o = ord(c)
        if c == '"':
            out.append('\\"')
        elif c == "\\":
            out.append("\\\\")
        elif c in "\n\t\r\b\f":
            out.append({"\n": "\\n", "\t": "\\t", "\r": "\\r", "\b": "\\b", "\f": "\\f"}[c])
        elif o < 0x20:
            out.append("\\u%04x" % o)
        else:
            out.append(c)
    return '"' + "".join(out) + '"'


def write(v):
    if v is None:
        return "null"
    if v is True:
        return "true"
    if v is False:
        return "false"
    if isinstance(v, int):
        return str(v) if -2**63 <= v < 2**63 else wfloat(float(v))
    if isinstance(v, float):
        return wfloat(v)
    if isinstance(v, str):
        return wstr(v)
    if isinstance(v, list):
        return "[" + ",".join(map(write, v)) + "]"
    return "{" + ",".join(wstr(k) + ":" + write(x) for k, x in v.items()) + "}"


def pairs(ps):
    d = {}
    for k, v in ps:
        d[k] = v  # first position, last value (dict keeps insertion order)
    return d


def expected_valid(text):
    return write(json.loads(text, object_pairs_hook=pairs))


VALID = [
    "null", "true", "false", "0", "-0", "-0.0", "1", "-1", "42", "3.5", "-2.25", "0.1",
    "1e2", "1E2", "1e+2", "1e-2", "2.5E-5", "6.02214076e23", "1.7976931348623157e308",
    "5e-324", "2.2250738585072014e-308", "9223372036854775807", "-9223372036854775808",
    "9223372036854775808", "-9223372036854775809", "123456789012345678901234567890",
    "0.30000000000000004", "3.141592653589793", "100.0", "1234567.125",
    '""', '"a"', '"hello world"', '"\\""', '"\\\\"', '"\\/"', '"\\b\\f\\n\\r\\t"',
    '"\\u0041"', '"\\u00e9"', '"\\u20ac"', '"\\ud83d\\ude00"', '"\\uD834\\uDD1E"', '"\\u0000"',
    '"\\u001f"', '"\\u007f"', '"é"', '"日本語"', '"😀"', '"mixed ascii ✓ and \\u2713"',
    "[]", "{}", "[[]]", "[{}]", '{"a":[]}', '{"a":{}}', "[1,2,3]", "[1, 2, 3]",
    '[null,true,false,0,"x",[],{}]', '{"a":1}', '{"a":1,"b":2}', '{"b":1,"a":2}',
    '{"a":1,"a":2}', '{"a":1,"b":2,"a":3}', '{"":0}', '{"\\u00e9":"é"}',
    ' \t\n\r[ 1 , 2 ] \n', '\n\n{\n  "a" : 1\n}\n', '[\r\n1\r\n]',
    '{"a":{"b":{"c":{"d":[1,[2,[3]]]}}}}', '[' * 50 + ']' * 50, '[' * 512 + ']' * 512,
    '{"name":"ana","age":3,"tags":["x","y"],"ok":true,"score":9.5,"none":null}',
]


def rand_str(r):
    alphabet = ["a", "b", "z", " ", "é", "€", "😀", "\n", "\t", '"', "\\", "/", "\x01", "\x1f", "日"]
    return "".join(r.choice(alphabet) for _ in range(r.randint(0, 8)))


def rand_num_text(r):
    k = r.randint(0, 5)
    if k == 0:
        return str(r.randint(-10**6, 10**6))
    if k == 1:
        return str(r.randint(-2**63, 2**63 - 1))
    if k == 2:
        return "%d.%d" % (r.randint(-1000, 1000), r.randint(0, 99999))
    if k == 3:
        return "%de%d" % (r.randint(1, 999), r.randint(-30, 30))
    if k == 4:
        return repr(r.uniform(-1e6, 1e6))
    return "%d.%dE%+d" % (r.randint(0, 9), r.randint(0, 999), r.randint(-300, 300))


def rand_doc(r, depth=0):
    k = r.randint(0, 7 if depth < 4 else 4)
    if k == 0:
        return r.choice(["null", "true", "false"])
    if k in (1, 2):
        return rand_num_text(r)
    if k in (3, 4):
        return json.dumps(rand_str(r), ensure_ascii=r.random() < 0.5)
    if k == 5:
        sep = r.choice([",", ", ", " ,\n "])
        return "[" + sep.join(rand_doc(r, depth + 1) for _ in range(r.randint(0, 4))) + "]"
    sep = r.choice([",", ", ", ",\n  "])
    keys = [r.choice(["a", "b", "c", "key", "é", ""]) for _ in range(r.randint(0, 4))]
    return "{" + sep.join(json.dumps(k) + r.choice([":", " : "]) + rand_doc(r, depth + 1) for k in keys) + "}"


# Invalid documents: (text, offset of the error) with the offset found by a marker `§`.
INVALID = [
    "§", "  §", "[1,2,§]", '{"a":1,§}', "[1 §2]", "§]", "§}", "[§", "{§", '{"a"§', '{"a":§',
    '{"a":§}', '{§a:1}', '{§1:1}', '{"a" §1}', '{"a":1 §"b":2}', "[1,§,2]", "[§,1]", "§,",
    "0§1", "-0§1", "0§0", "-§", "-§a", "1.§", "1.§e5", "1e§", "1e+§", "1E-§", "§.5", "§+1",
    "§NaN", "§Infinity", "-§Infinity", "0§x1F", "1§_000", "[1]§x", "1 §2", '"a"§"b"', "{}§{}",
    "[]§]", "§tru", "§nul", "§fals", "§True", "§NULL", "[§tr]", "§'a'", '"\\§x"', '"\\§a"',
    '"§\\ud800"', '"§\\ud800x"', '"§\\udc00"', '"a§\\ud800\\u0041"', '"§\\ud800\\ud800"',
    '"\\u12§"', '"\\u§"', '"\\u12§G4"', '"abc§', '"§', '["a§', '{"a§', '"a\\§', '"\\u00§',
    "§1e400", "§-1e400", "[§1e999]", '"a§\tb"', '"a§\nb"', '"§\x00"', '"x§\x1f"',
    "[" * 512 + "§[" + "]" * 513, "[" * 512 + "§" + "[" * 88 + "1" + "]" * 600, '{"a":' * 512 + '§{"a":1' + "}" * 513,
    "\n\n  §]", "[\n1,\n§,]", "{\n\"a\":1,\n§}", "[1,\r\n§]", "\t§x", "[1]\n\n  §2",
    "§ []", "§﻿[]", "[1§;2]", '{"a"§=1}', '{"a":1§;}', "[§'x']", "§/*c*/1", "1§//c",
    "[1,2§", '{"a":[1,2§}', '[{"a":1§]', '{"a":{"b":1}§', "[1,[2,[3]]§", '["a"§:1]',
    '{"a":1,"b"§}', '"\\\\\\"§', "§\x01", "[1§\x00]", "§nulx", "null§l", "true§x", "false§1",
    "0.5§.5", "1e5§e5", "-§+1", "-§-1", "0§x", "0§b1", "1E§", "§'", '§`x`',
]


def bad_utf8():
    # (bytes, error column): a lone continuation byte, a truncated sequence, an overlong form.
    return [(b'"\x80"', 2), (b'"a\xc3"', 3), (b'"\xff"', 2), (b'"\xe2\x82"', 2), (b'["\xc0\xaf"]', 3)]


def main():
    for kind in ("valid", "invalid"):
        os.makedirs(os.path.join(HERE, kind), exist_ok=True)
    r = random.Random(7)
    docs = list(VALID) + [rand_doc(r) for _ in range(60)]
    for i, d in enumerate(docs):
        base = os.path.join(HERE, "valid", "%03d" % i)
        open(base + ".json", "w", encoding="utf-8", newline="").write(d)
        open(base + ".expected", "w", encoding="utf-8").write(expected_valid(d) + "\n")
    n = 0
    for t in INVALID:
        off = t.index("§")
        text = t.replace("§", "", 1)
        line = text[:off].count("\n") + 1
        col = off - (text.rfind("\n", 0, off) + 1) + 1
        base = os.path.join(HERE, "invalid", "%03d" % n)
        open(base + ".json", "w", encoding="utf-8", newline="").write(text)
        open(base + ".expected", "w").write("line %d, column %d\n" % (line, col))
        n += 1
    for b, col in bad_utf8():
        base = os.path.join(HERE, "invalid", "%03d" % n)
        open(base + ".json", "wb").write(b)
        open(base + ".expected", "w").write("line 1, column %d\n" % col)
        n += 1
    print(f"{len(docs)} valid, {n} invalid")


if __name__ == "__main__":
    main()
