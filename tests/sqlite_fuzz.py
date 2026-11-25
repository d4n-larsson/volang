#!/usr/bin/env python3
"""Spec 026 SC-007: no SQL text, parameter list or database file stops a program by surprise.

Random statements (token soup and mutations of valid ones) go through `parse_sql` and `query` with
random parameter lists on an in-memory database; database files with random bytes changed are opened
and queried. Every outcome must be `ok` or `err`: the program must end normally.

  sqlite_fuzz.py [--count N] [--files N] [--seed S]

Defaults: 10,000 statements, 200 files. VO_BIN selects the build (as for bin/vo).
"""
import os
import random
import sqlite3
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CHUNK = 2000

SEEDS = [
    "create table t(a integer primary key, b text, c real, d blob)",
    "insert into t(b, c, d) values (?, ?, ?)",
    "select a, b, c, d from t where a > ? order by b",
    "select count(*), sum(c), group_concat(b) from t group by a % 3",
    "update t set b = upper(b) || ? where a = ?",
    "delete from t where c < ?",
    "create index i on t(b, c)",
    "select * from t join t as u on t.a = u.a + ?",
    "with recursive c(n) as (select 1 union all select n + 1 from c where n < 20) select n from c",
    "select json_extract(?, '$.a'), printf('%.3f', ?), substr(?, 2, 3)",
    "select case when ? then 1 else 2 end, coalesce(?, 3), cast(? as integer)",
    "insert into t values (null, 'x', 1.5, x'00') on conflict(a) do update set b = excluded.b",
    "select a from t where b like ? escape '\\' limit ? offset ?",
    "pragma table_info(t)", "explain select * from t", "savepoint s", "release s",
]
TOKENS = ["select", "from", "where", "insert", "into", "values", "update", "set", "delete", "create", "table",
          "index", "drop", "t", "a", "b", "c", "(", ")", ",", "*", "+", "-", "/", "%", "||", "=", "<", ">",
          "and", "or", "not", "null", "1", "-1", "2.5", "'x'", "''", "x'ff'", "?", "?", ":a", "$1", ";",
          "order", "by", "group", "having", "limit", "join", "on", "as", "case", "when", "then", "else",
          "end", "cast", "integer", "text", "real", "blob", "union", "all", "with", "recursive", "is", "in",
          "between", "like", "glob", "exists", "distinct", "count(*)", "max", "min", "abs", "length",
          "printf", "'%s'", "'%.*f'", "attach", "database", "detach", "pragma", "vacuum", "begin", "commit",
          "rollback", "returning", "\"t\"", "[t]", "`t`", "--c\n", "/*c*/", "1e308", "9223372036854775807",
          "zeroblob(100000000)", "randomblob(3)", "load_extension", "'now'", "date"]


def mutate(rng, s):
    k = rng.randrange(5)
    if k == 0:
        i = rng.randrange(len(s) + 1)
        return s[:i] + rng.choice(TOKENS) + s[i:]
    if k == 1 and s:
        i = rng.randrange(len(s))
        return s[:i] + s[i + 1:]
    if k == 2 and s:
        i, j = sorted(rng.randrange(len(s) + 1) for _ in range(2))
        return s[:i] + s[j:]
    if k == 3:
        return s + " " + rng.choice(TOKENS)
    return s.replace(rng.choice(["a", "b", "t", "?", "("]), rng.choice(TOKENS), 1)


def statement(rng):
    if rng.random() < 0.7:
        s = rng.choice(SEEDS)
        for _ in range(rng.choice([0, 0, 1, 2, 3])):
            s = mutate(rng, s)
        return s
    return " ".join(rng.choice(TOKENS) for _ in range(rng.randint(1, 14)))


def vo_str(s):
    out = []
    for ch in s:
        if ch in '"\\':
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def value(rng):
    k = rng.randrange(6)
    if k == 0:
        return "sqlite.Value.integer(%s)" % rng.choice(["0", "1", "-1", "9223372036854775807", "-9223372036854775807 - 1",
                                                 str(rng.randint(-1000, 1000))])
    if k == 1:
        return "sqlite.Value.real(%s)" % rng.choice(["0.5", "-2.25", "1000000.0", "0.0"])
    if k == 2:
        return "sqlite.Value.text(%s)" % vo_str(rng.choice(["", "x", "{\"a\": 1}", "ÅäÖ", "%", "' or 1=1 --"]))
    if k == 3:
        return "sqlite.Value.blob(bytes.from_ints([%s]))" % ", ".join(str(rng.randrange(256)) for _ in range(rng.randint(1, 4)))
    return "sqlite.Value.null"


HEAD = """import std.sqlite;
import std.bytes;

fn one(db: sqlite.Database, text: str, params: list[sqlite.Value]) -> int {
    match sqlite.parse_sql(text) {
        case ok(st) {
            match sqlite.query(db, st, params) {
                case ok(_) {
                    return 1;
                }
                case err(_) {
                    return 2;
                }
            }
        }
        case err(_) {
            return 3;
        }
    }
}
"""

STATEMENTS = HEAD + """
fn main() uses console {
    let db = sqlite.open_memory();
    let _ = one(db, "create table t(a integer primary key, b text, c real, d blob)", new list[sqlite.Value]);
    var counts = [0, 0, 0, 0];
CALLS    print(str(counts[1]) + " ok, " + str(counts[2]) + " err, " + str(counts[3]) + " parse err");
}
"""

FILES = HEAD + """
fn probe(path: str) -> int uses file.read("dbs/") {
    match sqlite.open_readonly(path) {
        case ok(db) {
            var n = 0;
            for text in ["select * from sqlite_schema", "select count(*), sum(length(b)) from t",
                         "select * from t order by b", "select * from u where x > 5", "pragma integrity_check",
                         "select b from t where b > 'm' order by b desc"] {
                set n = n + one_ro(db, text);
            }
            sqlite.close(db);
            return n;
        }
        case err(_) {
            return 0;
        }
    }
}

fn one_ro(db: sqlite.Database[uses file.read], text: str) -> int uses file.read("dbs/") {
    match sqlite.parse_sql(text) {
        case ok(st) {
            match sqlite.query(db, st, new list[sqlite.Value]) {
                case ok(_) {
                    return 1;
                }
                case err(_) {
                    return 0;
                }
            }
        }
        case err(_) {
            return 0;
        }
    }
}

fn main() uses console, file.read("dbs/") {
    var total = 0;
    for i in 0..COUNT {
        set total = total + probe("dbs/" + str(i) + ".db");
    }
    print(str(total) + " queries answered");
}
"""


def run(path, cwd):
    r = subprocess.run([VO, "run", path], cwd=cwd, capture_output=True, timeout=900)
    return r.returncode, r.stdout.decode("utf-8", "replace"), r.stderr.decode("utf-8", "replace")


def make_db(path, rng):
    db = sqlite3.connect(path)
    db.execute("pragma page_size=512")
    db.execute("create table t(a integer primary key, b text, c real, d blob)")
    db.execute("create index tb on t(b)")
    db.execute("create table u(x, y)")
    for i in range(300):
        db.execute("insert into t(b, c, d) values (?, ?, ?)", ("row %d %s" % (i, "x" * rng.randint(0, 80)), i / 7,
                                                               bytes(rng.randrange(256) for _ in range(8))))
        db.execute("insert into u values (?, ?)", (i, str(i)))
    db.commit()
    db.close()


def main():
    args = sys.argv[1:]

    def opt(name, default):
        return int(args[args.index(name) + 1]) if name in args else default

    count, files, seed = opt("--count", 10000), opt("--files", 200), opt("--seed", 1)
    rng = random.Random(seed)
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        done = 0
        totals = [0, 0, 0]
        while done < count:
            n = min(CHUNK, count - done)
            calls = []
            for _ in range(n):
                params = [value(rng) for _ in range(rng.randint(0, 4))]
                plist = "[" + ", ".join(params) + "]" if params else "new list[sqlite.Value]"
                calls.append("    let r%d = one(db, %s, %s);\n    set counts[r%d] = counts[r%d] + 1;\n" %
                             (len(calls), vo_str(statement(rng)), plist, len(calls), len(calls)))
            path = os.path.join(tmp, "s%d.vo" % done)
            with open(path, "w") as f:
                f.write(STATEMENTS.replace("CALLS", "".join(calls)))
            code, out, err = run(path, tmp)
            if code != 0:
                bad += 1
                print("FAIL statements chunk at %d exited %d:\n%s" % (done, code, err[:3000]))
            else:
                nums = [int(w) for w in out.split() if w.isdigit()]
                totals = [a + b for a, b in zip(totals, nums)]
            done += n
        os.makedirs(os.path.join(tmp, "dbs"))
        base = os.path.join(tmp, "base.db")
        make_db(base, rng)
        data = open(base, "rb").read()
        for i in range(files):
            b = bytearray(data)
            for _ in range(rng.randint(1, 20)):
                k = rng.randrange(len(b)) if rng.random() < 0.7 else rng.randrange(100)
                b[k] = rng.randrange(256)
            if rng.random() < 0.1:
                b = b[:rng.randrange(len(b))]
            with open(os.path.join(tmp, "dbs", "%d.db" % i), "wb") as f:
                f.write(bytes(b))
        path = os.path.join(tmp, "files.vo")
        with open(path, "w") as f:
            f.write(FILES.replace("COUNT", str(files)))
        code, out, err = run(path, tmp)
        if code != 0:
            bad += 1
            print("FAIL damaged files exited %d:\n%s" % (code, err[:3000]))
        print("sqlite_fuzz: %d statements (%d ok, %d err, %d parse err), %d damaged files (%s), %d failures" %
              (count, totals[0], totals[1], totals[2], files, out.strip(), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
