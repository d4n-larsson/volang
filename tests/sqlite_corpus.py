#!/usr/bin/env python3
"""Spec 026 SC-001 (research R12): SQLite's results in vo, the same on every platform.

Runs 500+ statements (every value type, casts, printf with float formats, math functions,
aggregates, string functions) through `parse_sql` and `query` on an in-memory database in one vo
program, and prints each result exactly (floats by their bits). A smoke comparison checks the
results without floats against Python's sqlite3 (statements Python's SQLite lacks, or that an older
SQLite answers differently, are counted, not failed).

  sqlite_corpus.py [--cross BIN...]

--cross runs the program with each build (through bin/vo, VO_BIN=BIN) and requires identical output.
Exit 1 on any cross difference or a statement that crashes the program.
"""
import os
import sqlite3
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

VALUES = ["0", "1", "-7", "9223372036854775807", "-9223372036854775808", "0.1", "-0.0", "2.5", "1e308",
          "4.9e-324", "'abc'", "'ÅäÖ €'", "''", "'12'", "' 1e3 '", "x'00ff10'", "null"]
UNARY = ["abs(X)", "typeof(X)", "length(X)", "upper(X)", "lower(X)", "hex(X)", "quote(X)", "round(X)",
         "round(X, 2)", "ceil(X)", "floor(X)", "trunc(X)", "sign(X)", "sqrt(X)", "exp(X)", "ln(X)", "log10(X)",
         "log2(X)", "pow(X, 0.5)", "sin(X)", "cos(X)", "tan(X)", "asin(X)", "acos(X)", "atan(X)", "atan2(X, 3)",
         "sinh(X)", "cosh(X)", "tanh(X)", "asinh(X)", "acosh(X)", "atanh(X)", "degrees(X)", "radians(X)",
         "mod(X, 3)", "trim(X)", "unicode(X)", "cast(X as integer)", "cast(X as real)", "cast(X as text)",
         "cast(X as blob)", "cast(X as numeric)", "-(X)", "X + 1", "X * 2", "X / 3", "X % 7", "X || 'z'",
         "printf('%.17g', X)", "printf('%f', X)", "printf('%e', X)", "printf('%d', X)", "printf('%s', X)",
         "printf('%10.3f|', X)", "X = 1", "X < 'a'", "X is null", "coalesce(X, 'none')", "nullif(X, 0)",
         "iif(X, 'yes', 'no')", "substr(X, 2)", "instr(X, 'b')", "replace(X, 'a', 'Z')", "X like 'A%'",
         "X glob '*c'"]
OTHER = [
    "select sum(column1), avg(column1), total(column1), min(column1), max(column1), count(column1), count(*) from (values (1), (2.5), (null), ('a'))",
    "select group_concat(column1, '-') from (values ('a'), ('b'), (null), ('c'))",
    "select sum(column1) from (values (9223372036854775807), (1))",
    "select total(column1) from (values (9223372036854775807), (1))",
    "select char(72, 105, 8364)", "select printf('%5s|%-5s|%05d', 'a', 'b', 42)",
    "select printf('%.3e %g %G %.0f', 12345.6789, 0.0001, 1e20, 2.5)",
    "select printf('%x %X %o %c', 255, 255, 8, 65)", "select printf('%q %Q', 'it''s', null)",
    "select 1e400, -1e400, 1e400 - 1e400", "select 0.1 + 0.2, 1.0 / 3, 2.0 / 3, 10 / 4, 10.0 / 4",
    "select 9223372036854775807 + 1", "select -9223372036854775808 - 1", "select 5 / 0, 5 % 0, 5.0 / 0",
    "select cast('9223372036854775808' as integer), cast('-9223372036854775809' as integer)",
    "select cast(1e19 as integer), cast(-1e19 as integer), cast(1.5 as integer)",
    "select round(2.5), round(-2.5), round(0.125, 2), round(1e300, 2)",
    "select 'a' < 'b', 'a' < 'B', 'a' = 'A' collate nocase, x'00' < 'a'",
    "select json_object('a', 1, 'b', 2.5, 'c', 'x')", "select json_extract('{\"a\": [1, 2.5]}', '$.a[1]')",
    "select json_array_length('[1,2,3]')", "select json('{\"x\": 1e5}')",
    "select ltrim('  x  '), rtrim('  x  '), trim('xxaxx', 'x')", "select length('ÅäÖ'), octet_length('ÅäÖ')",
    "select substr('hello', -3, 2), substr('hello', 0), substr('hello', 2, -1)",
    "select hex(zeroblob(4)), length(zeroblob(1000))", "select unhex('41ff'), unhex('zz')",
    "select format('%d-%s', 3, 'x')", "select concat('a', null, 1, 2.5), concat_ws('-', 'a', null, 'b')",
    "select pi(), exp(1), ln(10), log(100), log(2, 8)",
    "with recursive c(n) as (select 1 union all select n + 1 from c where n < 50) select sum(n * n), avg(n) from c",
    "with recursive c(n) as (select 1 union all select n + 1 from c where n < 30) select total(1.0 / n) from c",
    "select max(1, 2.5, 'a'), min(1, 2.5, 'a'), max(null, 1)",
    "select typeof(1), typeof(1.0), typeof('1'), typeof(x'01'), typeof(null)",
    "select 1 in (1, 2), 3 between 1 and 5, case when 1 then 'a' else 'b' end",
    "select quote(1e15), quote(1e16), quote(0.1), quote(123456789.123456789)",
    "select cast(0.1 as text), cast(1e15 as text), cast(1e16 as text), cast(-0.0 as text)",
    "select 1e15 || '', 1e-5 || '', 123456789012345678 * 1.0 || ''",
    "select row_number() over (order by column1), column1 from (values (3), (1), (2)) order by column1",
    "select column1, sum(column1) over (order by column1 rows between 1 preceding and current row) from (values (1), (2), (3))",
    "select date('2024-02-29', '+1 year'), time('12:00:00', '+90 minutes')",
    "select julianday('2000-01-01'), strftime('%Y-%j', '2024-12-31'), unixepoch('1970-01-02')",
    "select datetime(1700000000, 'unixepoch')", "select like('a%', 'ABC'), glob('a*', 'ABC')",
    "select soundex('Robert')", "select randomblob(4)", "select random()", "select current_date",
]


def statements():
    out = []
    for f in UNARY:
        for v in VALUES:
            out.append("select " + f.replace("X", v))
    return out + OTHER


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


PROGRAM = """import std.sqlite.open_memory;
import std.sqlite.parse_sql;
import std.sqlite.query;
import std.sqlite.Row;
import std.sqlite.Value;
import std.bytes.to_hex;
import std.bytes.from_text;

# A float exactly: its sign, 53-bit significand and binary exponent (scaling by 2 is exact).
fn exact(f: float) -> str {
    if f != f {
        return "nan";
    }
    if f == 0.0 {
        return str(f);
    }
    var sign = "";
    var x = f;
    if x < 0.0 {
        set sign = "-";
        set x = -x;
    }
    if x - x != x - x {
        return sign + "inf";
    }
    var e = 0;
    while x >= 2.0 {
        set x = x / 2.0;
        set e = e + 1;
    }
    while x < 1.0 {
        set x = x * 2.0;
        set e = e - 1;
    }
    return sign + str(int(x * float(4503599627370496))) + "p" + str(e);
}

fn show(v: Value) -> str {
    match v {
        case integer(i) {
            return "i:" + str(i);
        }
        case real(f) {
            return "r:" + exact(f);
        }
        case text(s) {
            return "t:" + to_hex(from_text(s));
        }
        case blob(b) {
            return "b:" + to_hex(b);
        }
        case null {
            return "n";
        }
    }
}

fn run(db: Database, text: str) -> str {
    match parse_sql(text) {
        case ok(st) {
            match query(db, st, new list[Value]) {
                case ok(rows) {
                    var out = "";
                    for row in rows {
                        for v in row.values {
                            set out = out + " " + show(v);
                        }
                        set out = out + " |";
                    }
                    return "ok" + out;
                }
                case err(e) {
                    return "err " + e;
                }
            }
        }
        case err(e) {
            return "perr " + e;
        }
    }
}

fn main() uses console {
    let db = open_memory();
    let statements = [STATEMENTS];
    for i in 0..len(statements) {
        print(str(i) + " " + run(db, statements[i]));
    }
}
"""


def program(stmts):
    src = PROGRAM.replace("STATEMENTS", ",\n        ".join(vo_str(s) for s in stmts))
    return src.replace("import std.sqlite.Row;", "import std.sqlite.Row;\nimport std.sqlite.Database;")


def run_vo(path, binary=None):
    env = dict(os.environ)
    if binary:
        env["VO_BIN"] = binary
    r = subprocess.run([VO, "run", path], capture_output=True, timeout=600, env=env)
    return r.returncode, r.stdout.decode("utf-8", "replace"), r.stderr.decode("utf-8", "replace")


def py_show(v):
    if v is None:
        return "n"
    if isinstance(v, bool):
        v = int(v)
    if isinstance(v, int):
        return "i:%d" % v
    if isinstance(v, float):
        return None
    if isinstance(v, bytes):
        return "b:" + v.hex()
    return "t:" + v.encode("utf-8", "surrogatepass").hex()


def python_results(stmts):
    db = sqlite3.connect(":memory:")
    out = []
    for s in stmts:
        try:
            rows = db.execute(s).fetchall()
            out.append(" ".join(" ".join(str(py_show(v)) for v in r) + " |" for r in rows))
        except Exception:
            out.append(None)
    return out


def main():
    args = sys.argv[1:]
    cross = args[args.index("--cross") + 1:] if "--cross" in args else []
    stmts = statements()
    fails = 0
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "corpus.vo")
        with open(path, "w") as f:
            f.write(program(stmts))
        code, out, err = run_vo(path)
        if code != 0:
            print("FAIL the corpus program exited %d: %s" % (code, err[:2000]))
            return 1
        lines = out.splitlines()
        if len(lines) != len(stmts):
            print("FAIL %d result lines for %d statements" % (len(lines), len(stmts)))
            return 1
        for b in cross:
            c2, o2, e2 = run_vo(path, b)
            if c2 != 0 or o2 != out:
                fails += 1
                for i, (x, y) in enumerate(zip(lines, o2.splitlines())):
                    if x != y:
                        print("FAIL %s differs at %s:\n  %s\n  %s" % (b, stmts[i], x, y))
                        break
                else:
                    print("FAIL %s exited %d: %s" % (b, c2, e2[:500]))
        py = python_results(stmts)
        same = differ = skipped = 0
        errs = sum(1 for l in lines if not l.split(" ", 1)[1].startswith("ok"))
        for i, l in enumerate(lines):
            body = l.split(" ", 1)[1]
            if py[i] is None or "None" in py[i] or "r:" in body or not body.startswith("ok"):
                skipped += 1
            elif body == "ok " + py[i] or (body == "ok" and py[i] == ""):
                same += 1
            else:
                differ += 1
                if "-v" in args:
                    print("python differs: %s\n  vo     %s\n  python %s" % (stmts[i], body, py[i]))
    print("sqlite_corpus: %d statements (%d err), python smoke: %d same, %d differ, %d not compared%s" %
          (len(stmts), errs, same, differ, skipped, ", %d builds compared" % len(cross) if cross else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
