#!/usr/bin/env python3
"""Spec 049: the postgres package (packages/postgres).

  postgres.py                 the groups that need no database (part of `make test`):
                              unit, connect, auth, failures, fuzz, replay, parity, docs
  postgres.py --server        also query, types, tx, hostile, move against a real PostgreSQL:
                              VO_PG_TEST=host:port:database:user:password
                              (default 127.0.0.1:54329:vo:vo:secret, the container of quickstart.md)
  postgres.py --group NAME    one group
  postgres.py --full          fuzz with 10,000 seeds instead of 300 (SC-006)
  postgres.py --server --time SC-007: connect, 100,000 rows read, 10,000 rows inserted (timing)

Each case writes a small vo program into a temporary project that has the package pinned, runs it
with bin/vo ($VO_BIN; under qemu for the other architectures) and compares what it prints. The
groups without a database talk to tests/postgres_server.py, a scripted peer.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1  # under qemu
CA = os.path.join(ROOT, "tests", "tls", "certs", "test-ca.pem")
SERVER = os.path.join(ROOT, "tests", "postgres_server.py")
PACKAGE = os.path.join(ROOT, "packages", "postgres")
MODULES = ["statement", "wire", "values", "scram", "link", "signin", "exchange", "client"]
failures = []
cases = 0

ITEMS = ["Changes", "Database", "Options", "Row", "Statement", "Value", "close", "connect", "exec", "get",
         "get_bytes", "get_float", "get_int", "get_str", "get_time", "is_null", "parse_sql", "query", "sql",
         "transaction"]
PRELUDE = "".join(f"import postgres.client.{n};\n" for n in ITEMS) + """import std.bytes.from_ints;
import std.bytes.to_hex;

fn options(port: int, password: str, plain: bool, ms: int) -> Options {
    return new Options {
        host: "{HOST}",
        port: port,
        database: "{DB}",
        user: "{USER}",
        password: password,
        plain_text: plain,
        connect_ms: ms,
        statement_ms: ms
    };
}

fn shown(v: Value) -> str {
    match v {
        case integer(i) {
            return "integer " + str(i);
        }
        case real(f) {
            return "real " + str(f);
        }
        case text(s) {
            return "text " + s;
        }
        case blob(b) {
            return "blob " + to_hex(b);
        }
        case null {
            return "null";
        }
    }
}

fn did(r: result[Changes]) -> str {
    match r {
        case ok(c) {
            return "changed " + str(c.changed) + " last_id " + str(c.last_id);
        }
        case err(e) {
            return e;
        }
    }
}

fn rows(r: result[list[Row]]) -> str {
    match r {
        case ok(rs) {
            var lines = new list[str];
            for row in rs {
                var cells = new list[str];
                for i in 0..len(row.columns) {
                    push(cells, row.columns[i] + "=" + shown(row.values[i]));
                }
                push(lines, join(cells, ", "));
            }
            return str(len(rs)) + " rows: " + join(lines, " | ");
        }
        case err(e) {
            return e;
        }
    }
}

fn none() -> list[Value] {
    return new list[Value];
}
"""

# A program: `work` runs on an open connection; `main` connects with OPTIONS.
PROGRAM = PRELUDE + """
{EXTRA}
fn work[D: effects](db: Database[D]) uses console, D {
{BODY}
}

fn main() uses console, net("{HOST}:{P}"), entropy {
    match connect({OPTIONS}) {
        case ok(db) {
            print("connected");
            work(db);
            close(db);
        }
        case err(e) {
            print(e);
        }
    }
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def lines(name, got, want):
    case(name, got == want, f"\n  got:  {got}\n  want: {want}")


class Peer:
    """tests/postgres_server.py with a script, on a free port."""

    def __init__(self, script, *args):
        self.p = subprocess.Popen([sys.executable, SERVER, script] + list(args), stdout=subprocess.PIPE, text=True)
        self.port = int(self.p.stdout.readline().split()[1])

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.stop()

    def stop(self):
        if self.p.poll() is None:
            self.p.kill()
        self.p.wait()
        self.p.stdout.close()


class Target:
    """Where programs connect: the scripted peer on 127.0.0.1, or the real server of VO_PG_TEST."""

    def __init__(self, host="127.0.0.1", port=0, db="vo", user="vo", password="secret"):
        self.host, self.port, self.db, self.user, self.password = host, port, db, user, password


def real():
    host, port, db, user, password = os.environ.get("VO_PG_TEST", "127.0.0.1:54329:vo:vo:secret").split(":", 4)
    return Target(host, int(port), db, user, password)


def program(body, t, options=None, extra="", ms=5000, plain=True, password=None, template=PROGRAM):
    password = t.password if password is None else password
    options = options or f'options({t.port}, "{password}", {"true" if plain else "false"}, {ms * SLOW})'
    src = template.replace("{EXTRA}", extra).replace("{BODY}", body).replace("{OPTIONS}", options)
    return src.replace("{HOST}", t.host).replace("{P}", str(t.port)).replace("{DB}", t.db).replace("{USER}", t.user)


def vo(d, words, timeout=120):
    r = subprocess.run([VO] + words, cwd=d, capture_output=True, text=True, timeout=timeout * SLOW)
    return r.returncode, r.stdout.splitlines(), r.stderr


def run(d, src, name="p.vo", extra=(), timeout=120):
    with open(os.path.join(d, name), "w") as f:
        f.write(src)
    return vo(d, ["run", "--timeout", str(60 * SLOW)] + list(extra) + [name], timeout)


def project():
    """A temporary project with the package pinned as `postgres` (in pkg/postgres)."""
    d = tempfile.mkdtemp(prefix="vo-postgres-")
    shutil.copytree(PACKAGE, os.path.join(d, "pkg", "postgres"))
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write("fn main() {\n}\n")
    code, out, err = vo(d, ["package", "main.vo", "postgres", "pkg/postgres"])
    if code != 0:
        print("FAIL cannot pin the package:", out, err)
        sys.exit(1)
    return d


# ---------------------------------------------------------------- unit: the package's own test items

def group_unit(d):
    for m in MODULES:
        code, out, err = vo(d, ["test", f"pkg/postgres/{m}.vo"])
        last = out[-1] if out else ""
        case(f"unit/{m}", code == 0 and re.fullmatch(r"\d+ passed, 0 failed", last) is not None,
             f"{code} {last} {[l for l in out if l.startswith('FAIL')]} {err[-300:]}")
    code, out, err = vo(d, ["fmt", "--check"] + [f"pkg/postgres/{m}.vo" for m in MODULES])
    case("unit/formatted", code == 0, " ".join(out))
    code, out, err = vo(d, ["check", "pkg/postgres/client.vo"])
    warnings = [l for l in out if "warning[" in l and "W0504" not in l]
    case("unit/no warnings", code == 0 and not warnings, f"{code} {warnings}")


# ---------------------------------------------------------------- query, types (a real server)

QUERY = """    print(did(exec(db, sql("drop table if exists vo_notes"), none())));
    print(did(exec(db, sql("create table vo_notes (id serial primary key, body text not null unique, done boolean)"), none())));
    print(did(exec(db, sql("insert into vo_notes (body, done) values (?, ?), (?, ?), (?, ?)"),
        [Value.text("one"), Value.integer(0), Value.text("it's; -- ? $1"), Value.integer(1), Value.text("three"), Value.null])));
    print(rows(query(db, sql("select id, body, done from vo_notes where id >= ? order by id"), [Value.integer(2)])));
    print(did(exec(db, sql("update vo_notes set done = ? where id < ?"), [Value.integer(1), Value.integer(10)])));
    print(rows(query(db, sql("insert into vo_notes (body) values (?), (?) returning id"), [Value.text("a"), Value.text("b")])));
    print(did(exec(db, sql("insert into vo_notes (body) values (?)"), [Value.text("one")])));
    print(rows(query(db, sql("select count(*) as n from vo_notes"), none())));
    print(rows(query(db, sql("select 1 as x where false"), none())));
    print(rows(query(db, sql("select nope from vo_notes"), none())));
    print(did(exec(db, sql("select ?, ?"), [Value.integer(1)])));
    print(did(exec(db, sql("select 1; select 2"), none())));
    print(did(exec(db, sql("select $1"), none())));
    print(rows(query(db, sql("select ?::text as a, 'x ?? y' as b, '?' as c"), [Value.text("p")])));
    match query(db, sql("select id, body, done, null::int as missing from vo_notes where id = ?"), [Value.integer(1)]) {
        case ok(rs) {
            print(str(get_int(rs[0], "id") == ok(1)) + " " + str(get_str(rs[0], "body") == ok("one")));
            print(str(get_int(rs[0], "body") == err("body is text, not integer")));
            print(str(get_str(rs[0], "id") == err("id is integer, not text")));
            print(str(is_null(rs[0], "missing") == ok(true)) + " " + str(is_null(rs[0], "id") == ok(false)));
            print(str(get_int(rs[0], "missing") == err("missing is null, not integer")));
            print(str(get_int(rs[0], "nope") == err("no column nope")));
        }
        case err(e) {
            print(e);
        }
    }
    print(did(exec(db, sql("drop table vo_notes"), none())));"""
QUERY_OUT = [
    "connected",
    "changed 0 last_id 0",
    "changed 0 last_id 0",
    "changed 3 last_id 0",
    "2 rows: id=integer 2, body=text it's; -- ? $1, done=integer 1 | id=integer 3, body=text three, done=null",
    "changed 3 last_id 0",
    "2 rows: id=integer 4 | id=integer 5",
    'constraint: 23505 duplicate key value violates unique constraint "vo_notes_body_key"',
    "1 rows: n=integer 5",
    "0 rows: ",
    'error: 42703 column "nope" does not exist (at position 8)',
    "error: the statement has 2 parameters, and 1 values were given",
    "error: one statement per sql",
    "error: write parameters as ?, not $1",
    "1 rows: a=text p, b=text x ?? y, c=text ?",  # (inside a string `??` is two characters)
    "true true", "true", "true", "true true", "true", "true",
    "changed 0 last_id 0",
]

TYPES = """    print(rows(query(db, sql("select 1::smallint as a, 2::integer as b, 9007199254740993::bigint as c, true as d, false as e"), none())));
    print(rows(query(db, sql("select 1.5::real as a, 0.1::double precision as b, 1e300::double precision as c, 'NaN'::double precision as d, '-Infinity'::real as e"), none())));
    print(rows(query(db, sql("select 'héllo'::text as a, 'v'::varchar(5) as b, 'c'::char(3) as c, ''::text as d"), none())));
    print(rows(query(db, sql("select '\\\\x00ff10'::bytea as a, ''::bytea as b"), none())));
    print(rows(query(db, sql("select 12345678901234567890.123456789::numeric as a, 0.10::numeric(10,2) as b"), none())));
    print(rows(query(db, sql("select '2026-11-23 10:00:00.123456+02'::timestamptz as a, '2026-11-23 10:00:00'::timestamp as b, '2026-11-23'::date as c, '10:00:00'::time as d"), none())));
    print(rows(query(db, sql("select 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid as a, '{\\"k\\": [1, 2]}'::jsonb as b, array[1, 2, 3] as c, '1 day 02:00:00'::interval as d"), none())));
    print(rows(query(db, sql("select null as a, null::int as b, null::text as c"), none())));
    print(rows(query(db, sql("select ?::bigint as a, ?::double precision as b, ?::text as c, ?::bytea as d, ?::int as e, ?::boolean as f, ?::numeric as g"),
        [Value.integer(-42), Value.real(2.5), Value.text("téxt"), Value.blob(from_ints([0, 255, 39, 92])), Value.null, Value.integer(1), Value.text("1.10")])));
    print(rows(query(db, sql("select ?::timestamptz as a, ?::int as b"), [Value.text("2026-11-23T10:00:00.500Z"), Value.text("12")])));
    print(rows(query(db, sql("select ?::int as a"), [Value.text("abc")])));
    print(rows(query(db, sql("select ?::text as a"), [Value.text("a" + chr(0) + "b")])));
    match query(db, sql("select '2026-11-23 10:00:00.123456+00'::timestamptz as a, '1970-01-02'::date as b, now() as c, 1.5::double precision as d"), none()) {
        case ok(rs) {
            print(str(get_time(rs[0], "a") == ok(1795428000123)) + " " + str(get_time(rs[0], "b") == ok(86400000)));
            match get_time(rs[0], "c") {
                case ok(ms) {
                    print(str(ms > 1700000000000));
                }
                case err(e) {
                    print(e);
                }
            }
            print(str(get_float(rs[0], "d") == ok(1.5)) + " " + str(get_time(rs[0], "d") == err("d is real, not a date or time")));
        }
        case err(e) {
            print(e);
        }
    }"""
TYPES_OUT = [
    "connected",
    "1 rows: a=integer 1, b=integer 2, c=integer 9007199254740993, d=integer 1, e=integer 0",
    "1 rows: a=real 1.5, b=real 0.1, c=real 1" + "0" * 300 + ".0, d=text NaN, e=text -Infinity",
    "1 rows: a=text héllo, b=text v, c=text c  , d=text ",
    "1 rows: a=blob 00ff10, b=blob ",
    "1 rows: a=text 12345678901234567890.123456789, b=text 0.10",
    "1 rows: a=text 2026-11-23 08:00:00.123456+00, b=text 2026-11-23 10:00:00, c=text 2026-11-23, d=text 10:00:00",
    '1 rows: a=text a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11, b=text {"k": [1, 2]}, c=text {1,2,3}, d=text 1 day 02:00:00',
    "1 rows: a=null, b=null, c=null",
    "1 rows: a=integer -42, b=real 2.5, c=text téxt, d=blob 00ff275c, e=null, f=integer 1, g=text 1.10",
    "1 rows: a=text 2026-11-23 10:00:00.5+00, b=integer 12",
    'error: 22P02 invalid input syntax for type integer: "abc"',
    "error: text with a zero byte cannot be stored: pass it as a blob",
    "true true", "true", "true true",
]


def group_query(d):
    t = real()
    code, out, err = run(d, program(QUERY, t))
    lines("query/store and read", (code, out), (0, QUERY_OUT))


def group_types(d):
    t = real()
    code, out, err = run(d, program(TYPES, t))
    lines("types/every kind in and out", (code, out), (0, TYPES_OUT))


# ---------------------------------------------------------------- hostile parameters (a real server)

HOSTILE = """    print(did(exec(db, sql("drop table if exists vo_hostile"), none())));
    print(did(exec(db, sql("create table vo_hostile (n int primary key, s text)"), none())));
    let parts = ["'", "''", "\\"", ";", "--", "/*", "*/", "$$", "$1", "?", "??", "\\\\", "drop table vo_hostile", "\\n", "\\t",
        "é", "\\u{1F600}", "%", "_", "E'", "\\\\'", ")", "(", ", ", " or 1=1 ", "\\\\x", "null", "\\r"];
    var bad = 0;
    for n in 0..200 {
        var s = "";
        for k in 0..(1 + n % 5) {
            set s = s + parts[(n * 7 + k * 13 + n / 5) % len(parts)];
        }
        match exec(db, sql("insert into vo_hostile (n, s) values (?, ?)"), [Value.integer(n), Value.text(s)]) {
            case ok(_) {
            }
            case err(e) {
                print(e);
            }
        }
        match query(db, sql("select s from vo_hostile where n = ? and s = ?"), [Value.integer(n), Value.text(s)]) {
            case ok(rs) {
                if len(rs) != 1 or get_str(rs[0], "s") != ok(s) {
                    set bad = bad + 1;
                }
            }
            case err(e) {
                print(e);
            }
        }
    }
    print("mismatches " + str(bad));
    print(rows(query(db, sql("select count(*) as n from vo_hostile"), none())));
    print(did(exec(db, sql("drop table vo_hostile"), none())));"""


def group_hostile(d):
    t = real()
    code, out, err = run(d, program(HOSTILE, t))
    lines("hostile/200 strings as parameters come back byte for byte (SC-004)", (code, out),
          (0, ["connected", "changed 0 last_id 0", "changed 0 last_id 0", "mismatches 0", "1 rows: n=integer 200",
               "changed 0 last_id 0"]))
    code, out, err = run(d, program('    print("in");', t, password="wrong"))
    case("hostile/the real server refuses a wrong password", code == 0 and len(out) == 1 and out[0].startswith("auth: 28P01 "),
         f"{code} {out}")
    case("hostile/the wrong password is not in the error", "wrong" not in " ".join(out), str(out))


# ---------------------------------------------------------------- transactions (a real server)

TX_EXTRA = """
fn both[D: effects](db: Database[D], fail: bool) -> result[int] uses D {
    let _ = try exec(db, sql("update vo_stock set n = n - 1 where place = ?"), [Value.text("a")]);
    if fail {
        return err("out of stock");
    }
    let _ = try exec(db, sql("update vo_stock set n = n + 1 where place = ?"), [Value.text("b")]);
    return ok(7);
}

fn shown_int(r: result[int]) -> str {
    match r {
        case ok(n) {
            return "ok " + str(n);
        }
        case err(e) {
            return e;
        }
    }
}

fn stock[D: effects](db: Database[D]) -> str uses D {
    return rows(query(db, sql("select place, n from vo_stock order by place"), none()));
}
"""
TX = """    print(did(exec(db, sql("drop table if exists vo_stock"), none())));
    print(did(exec(db, sql("create table vo_stock (place text primary key, n int)"), none())));
    print(did(exec(db, sql("insert into vo_stock values ('a', 5), ('b', 5)"), none())));
    match connect({OPTIONS}) {
        case ok(other) {
            print(shown_int(transaction(db, fn() -> result[int] {
                let r = try both(db, false);
                print("inside, the other connection sees: " + stock(other));
                return ok(r);
            })));
            print("after: " + stock(other));
            print(shown_int(transaction(db, fn() -> result[int] {
                return both(db, true);
            })));
            print("after an err: " + stock(other));
            print(shown_int(transaction(db, fn() -> result[int] {
                return transaction(db, fn() -> result[int] {
                    return ok(1);
                });
            })));
            print(shown_int(transaction(db, fn() -> result[int] {
                print(did(exec(db, sql("insert into vo_stock values ('a', 1)"), none())));
                print(did(exec(db, sql("update vo_stock set n = 0"), none())));
                return ok(2);
            })));
            print("after a failed statement: " + stock(other));
            print(did(exec(db, sql("update vo_stock set n = 9 where place = 'a'"), none())));
            close(other);
        }
        case err(e) {
            print(e);
        }
    }
    print(did(exec(db, sql("drop table vo_stock"), none())));"""
TX_OUT = [
    "connected",
    "changed 0 last_id 0", "changed 0 last_id 0", "changed 2 last_id 0",
    "inside, the other connection sees: 2 rows: place=text a, n=integer 5 | place=text b, n=integer 5",
    "ok 7",
    "after: 2 rows: place=text a, n=integer 4 | place=text b, n=integer 6",
    "out of stock",
    "after an err: 2 rows: place=text a, n=integer 4 | place=text b, n=integer 6",
    "error: a transaction is already open",
    'constraint: 23505 duplicate key value violates unique constraint "vo_stock_pkey"',
    "error: 25P02 current transaction is aborted, commands ignored until end of transaction block",
    "error: a statement in the transaction failed",
    "after a failed statement: 2 rows: place=text a, n=integer 4 | place=text b, n=integer 6",
    "changed 1 last_id 0",
    "changed 0 last_id 0",
]
TX_STOP = """    print(did(exec(db, sql("drop table if exists vo_stop"), none())));
    print(did(exec(db, sql("create table vo_stop (n int)"), none())));
    let xs = new list[int];
    let r = transaction(db, fn() -> result[int] {
        let _ = try exec(db, sql("insert into vo_stop values (1)"), none());
        return ok(xs[3]);
    });
    print("not reached");"""
TX_AFTER = """    print(rows(query(db, sql("select count(*) as n from vo_stop"), none())));
    print(did(exec(db, sql("drop table vo_stop"), none())));"""


def group_tx(d):
    t = real()
    src = program(TX, t, extra=TX_EXTRA).replace("uses console, D {", "uses console, net, entropy, D {")  # a second connection
    code, out, err = run(d, src)
    lines("tx/commit, rollback, nesting, a failed statement", (code, out), (0, TX_OUT))
    code, out, err = run(d, program(TX_STOP, t))
    case("tx/a runtime error midway stops the program", code == 70 and "R0003" in err and out[-1] == "changed 0 last_id 0",
         f"{code} {out} {err[:200]}")
    code, out, err = run(d, program(TX_AFTER, t))
    lines("tx/and leaves no change", (code, out), (0, ["connected", "1 rows: n=integer 0", "changed 0 last_id 0"]))


# ---------------------------------------------------------------- connect, auth (the scripted peer)

ONE = '    print(rows(query(db, sql("select 1 as x"), none())));'
ONE_OUT = ["connected", "1 rows: x=integer 1"]


def group_connect(d):
    ca = ["--ca-file", CA]
    with Peer("tls-ok") as p:
        code, out, err = run(d, program(ONE, Target(port=p.port), plain=False), extra=ca)
        lines("connect/TLS by default, verified", (code, out), (0, ONE_OUT))
        code, out, err = run(d, program(ONE, Target(port=p.port), plain=False))
        case("connect/an unknown authority is refused", code == 0 and len(out) == 1 and out[0].startswith("tls: "), f"{code} {out}")
    capture = os.path.join(d, "capture.bin")
    with Peer("plain-ok", "--capture", capture) as p:
        code, out, err = run(d, program(ONE, Target(port=p.port), plain=False), extra=ca)
        lines("connect/a server without TLS is refused", (code, out), (0, ["tls: the server does not offer TLS"]))
        time.sleep(0.2)
        sent = open(capture, "rb").read()
        case("connect/and nothing but the TLS request was sent", sent == bytes([0, 0, 0, 8, 4, 210, 22, 47]), sent.hex())
        code, out, err = run(d, program(ONE, Target(port=p.port), plain=True))
        lines("connect/plain_text, set in the options", (code, out), (0, ONE_OUT))
    with Peer("tls-bad-name") as p:
        code, out, err = run(d, program(ONE, Target(port=p.port), plain=False), extra=ca)
        case("connect/a certificate for another name is refused", code == 0 and len(out) == 1 and out[0].startswith("tls: "),
             f"{code} {out}")
    with Peer("plain-ok") as p:
        src = program(ONE, Target(port=p.port)).replace(f'net("127.0.0.1:{p.port}")', 'net("127.0.0.1:1")')
        code, out, err = run(d, src)
        case("connect/a host outside the grant is R0010 (spec 050)", code == 70 and "R0010" in err and f"127.0.0.1:{p.port}" in err,
             f"{code} {out} {err[:300]}")
        src = program(ONE, Target(port=p.port)).replace("fn work[D: effects](db: Database[D]) uses console, D {",
                                                        "fn work(db: Database[uses net]) uses console {")
        with open(os.path.join(d, "e.vo"), "w") as f:
            f.write(src)
        code, out, err = vo(d, ["check", "e.vo"])
        case("connect/using a database needs its effect (E0501)", code == 65 and any("E0501" in l and "net" in l for l in out),
             f"{code} {out[:3]}")
    src = program(ONE, Target(port=1), options='options(0, "x", true, 1000)')
    code, out, err = run(d, src)
    lines("connect/a port out of range", (code, out), (0, ["error: the port must be 1 to 65535"]))


def group_auth(d):
    secret = "s3cret-Pass-W0RD"
    capture = os.path.join(d, "auth.bin")
    with Peer("scram-ok", "--password", secret, "--capture", capture) as p:
        t = Target(port=p.port, password=secret)
        code, out, err = run(d, program(ONE, t))
        lines("auth/SCRAM-SHA-256", (code, out), (0, ONE_OUT))
        time.sleep(0.2)
        sent = open(capture, "rb").read()
        case("auth/the password is in no byte sent (SC-004)", secret.encode() not in sent and len(sent) > 100, f"{len(sent)} bytes")
        code, out, err = run(d, program(ONE, t, password="another"))
        lines("auth/a wrong password", (code, out), (0, ['auth: 28P01 password authentication failed for user "vo"']))
    with Peer("scram-wrong-password", "--password", secret) as p:
        code, out, err = run(d, program(ONE, Target(port=p.port, password=secret)))
        case("auth/the password is not in the error", code == 0 and out and out[0].startswith("auth: 28P01") and secret not in out[0],
             f"{code} {out}")
    with Peer("scram-bad-signature", "--password", secret) as p:
        code, out, err = run(d, program(ONE, Target(port=p.port, password=secret)))
        lines("auth/a server that cannot prove it knows the password", (code, out),
              (0, ["auth: the server did not prove that it knows the password"]))
    with Peer("no-proof", "--password", secret) as p:
        code, out, err = run(d, program(ONE, Target(port=p.port, password=secret)))
        lines("auth/a server that skips its proof", (code, out), (0, ["auth: the server did not prove that it knows the password"]))
    with Peer("asks-md5") as p:
        code, out, err = run(d, program(ONE, Target(port=p.port)))
        lines("auth/MD5 is not supported", (code, out),
              (0, ["auth: the server asked for an MD5 password; only SCRAM-SHA-256 is supported"]))
    with Peer("asks-cleartext") as p:
        code, out, err = run(d, program(ONE, Target(port=p.port)))
        lines("auth/a clear-text password is never sent", (code, out),
              (0, ["auth: the server asked for a clear-text password; only SCRAM-SHA-256 is supported"]))


# ---------------------------------------------------------------- failures, fuzz (the scripted peer)

AGAIN = ONE + "\n" + ONE


def free_port():
    import socket
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def group_failures(d):
    code, out, err = run(d, program(ONE, Target(port=free_port())))
    lines("failures/no server", (code, out), (0, ["connection: the connection was refused"]))
    with Peer("stalls-connect") as p:
        t0 = time.time()
        code, out, err = run(d, program(ONE, Target(port=p.port), ms=300))
        took = time.time() - t0
        lines("failures/a server that says nothing", (code, out), (0, ["timeout: the server did not answer within the time limit"]))
        case("failures/within the connect limit", took < 5 * SLOW, f"{took:.1f} s")
    with Peer("stalls-query") as p:
        t0 = time.time()
        code, out, err = run(d, program(AGAIN, Target(port=p.port), ms=300))
        took = time.time() - t0
        lines("failures/a statement over its limit closes the connection", (code, out),
              (0, ["connected", "timeout: the server did not answer within the time limit", "connection: closed"]))
        case("failures/within the statement limit", took < 5 * SLOW, f"{took:.1f} s")
    with Peer("closes-in-query") as p:
        code, out, err = run(d, program(AGAIN, Target(port=p.port)))
        case("failures/the server closes in a statement",
             code == 0 and len(out) == 3 and out[1].startswith("connection: ") and out[2] == "connection: closed", f"{code} {out}")
    with Peer("closes-in-commit") as p:
        body = """    match transaction(db, fn() -> result[int] {
        return ok(1);
    }) {
        case ok(_) {
            print("committed");
        }
        case err(e) {
            print(e);
        }
    }"""
        code, out, err = run(d, program(body, Target(port=p.port)))
        case("failures/the server closes in a commit",
             code == 0 and len(out) == 2 and out[1].startswith("connection: ")
             and out[1].endswith("; whether the transaction was committed is not known"), f"{code} {out}")
    with Peer("server-error") as p:
        code, out, err = run(d, program('    print(rows(query(db, sql("select ?, nope"), [Value.integer(1)])));\n' + ONE, Target(port=p.port)))
        lines("failures/a server error: name, SQLSTATE, message, position; the connection stays usable", (code, out),
              (0, ["connected", 'error: 42703 column "nope" does not exist (at position 8)',
                   'error: 42703 column "nope" does not exist (at position 8)']))


def group_fuzz(d, full=False):
    n = 10000 if full else 300
    bad = 0
    kinds = {}
    src_for = lambda port: program(AGAIN, Target(port=port), ms=400)
    for seed in range(1, n + 1):
        with Peer("garbage", "--seed", str(seed)) as p:
            t0 = time.time()
            code, out, err = run(d, src_for(p.port))
            took = time.time() - t0
        ok = code == 0 and len(out) == 3 and out[0] == "connected" and took < 10 * SLOW
        first = out[1].split(":")[0] if len(out) > 1 else "?"
        kinds[first] = kinds.get(first, 0) + 1
        # every malformed answer is an err value; none is a row, a runtime error or a hang
        ok = ok and first in ("protocol", "connection", "timeout", "error") and "rows:" not in out[1]
        if not ok:
            bad += 1
            if bad <= 5:
                print(f"FAIL fuzz/seed {seed}: {code} {out} {err[:200]} {took:.1f} s")
    case(f"fuzz/{n} malformed answers are err values within the time limit (SC-006)", bad == 0, f"{bad} failures; {kinds}")
    print(f"postgres: fuzz {n} seeds: {kinds}")


# ---------------------------------------------------------------- replay (the scripted peer)

def group_replay(d):
    secret = "r3play-Pass-W0RD"
    p = Peer("scram-ok", "--password", secret)
    src = program(ONE, Target(port=p.port, password=secret))
    code, out, err = run(d, src, "r.vo", extra=["--record", "r.rec"])
    lines("replay/the recorded run", (code, out), (0, ONE_OUT))
    p.stop()
    code, out, err = vo(d, ["replay", "r.rec"])
    lines("replay/the same output with the server stopped (SC-005)", (code, out), (0, ONE_OUT))
    text = open(os.path.join(d, "r.rec"), errors="replace").read()
    case("replay/the recording holds no password", secret not in text, "found")
    q = Peer("plain-ok")
    test = PRELUDE.replace("{HOST}", "127.0.0.1").replace("{DB}", "vo").replace("{USER}", "vo") + f"""
fn main() {{
}}

test "a query against the test server" uses net("127.0.0.1:{q.port}"), entropy {{
    match connect(options({q.port}, "", true, 2000)) {{
        case ok(db) {{
            expect_eq(rows(query(db, sql("select 1 as x"), none())), "1 rows: x=integer 1");
            close(db);
        }}
        case err(e) {{
            expect_eq(e, "");
        }}
    }}
}}
"""
    with open(os.path.join(d, "t.vo"), "w") as f:
        f.write(test)
    code, out, err = vo(d, ["test", "t.vo"])
    case("replay/vo test against a server", code == 0 and "PASS a query against the test server" in " ".join(out)
         and out[-1].endswith(" 0 failed"), f"{code} {out[-3:]} {err[:200]}")
    q.stop()
    code, out, err = vo(d, ["test", "t.vo"])
    case("replay/vo test with no server fails with the reason", code != 0 and any("connection: " in l for l in out),
         f"{code} {out}")


# ---------------------------------------------------------------- parity with std.sqlite (SC-003), docs

SHARED = ["Statement", "Value", "Row", "Changes", "sql", "parse_sql", "exec", "query", "transaction", "get", "get_int",
          "get_float", "get_str", "get_bytes", "is_null"]


def signatures(d, module):
    """The signature line of each item `vo doc MODULE` prints."""
    code, out, err = vo(d, ["doc", module])
    found = {}
    for line in out:
        m = re.match(r"(?:fn|record|variant) (\w+)", line)
        if m:
            found[m.group(1)] = re.sub(r"\s+", " ", line.strip())
    return found


def group_parity(d):
    ours, theirs = signatures(d, "postgres.client"), signatures(d, "std.sqlite")
    for name in SHARED:
        a, b = ours.get(name), theirs.get(name)
        if name == "Value":  # the same cases; the comments name each database's types
            a, b = re.sub(r"#.*", "", a or ""), re.sub(r"#.*", "", b or "")
        case(f"parity/{name}", a is not None and a == b, f"\n  postgres.client: {a}\n  std.sqlite:      {b}")
    close = ours.get("close", "")
    case("parity/close differs only by its effect", close.replace(" uses E", "") == theirs.get("close"), f"{close} | {theirs.get('close')}")
    case("parity/Database has an effect parameter", ours.get("Database", "").startswith("record Database[E: effects]"), ours.get("Database"))


def top_level(text):
    """text split at each `; ` outside brackets, braces and strings."""
    parts, depth, quoted, start, i = [], 0, False, 0, 0
    while i < len(text):
        c = text[i]
        if c == '"' and (i == 0 or text[i - 1] != "\\"):
            quoted = not quoted
        elif not quoted and c in "([{":
            depth += 1
        elif not quoted and c in ")]}":
            depth -= 1
        elif not quoted and depth == 0 and text.startswith("; ", i):
            parts.append(text[start:i])
            start = i + 2
        i += 1
    return parts + [text[start:]]


def group_docs(d):
    code, out, err = vo(d, ["doc", "--json", "postgres.client"])
    missing = []
    examples = []
    import json
    seen = 0
    for line in out:
        try:
            item = json.loads(line)
        except ValueError:
            continue
        if item.get("kind") != "doc":
            continue
        seen += 1
        if not item.get("description") or not item.get("example"):
            missing.append(item.get("name"))
        else:
            examples.append((item["name"], item["example"]))
    case("docs/every exported item has a comment and an example", seen >= len(ITEMS) and not missing, f"{seen} items, missing: {missing}")
    # each example, `[STATEMENTS; ]EXPR is VALUE`, becomes a test that checks EXPR == VALUE
    tests = []
    for name, ex in examples:
        parts = top_level(ex)
        expr, _, value = parts[-1].rpartition(" is ")
        body = "".join(f"    {st};\n" for st in parts[:-1])
        grant = " uses net, entropy" if name == "connect" else ""  # (it fails before connecting)
        tests.append(f'test "example of {name}"{grant} {{\n{body}    expect_eq({expr}, {value});\n}}\n')
    imports = "".join(f"import postgres.client.{n};\n" for n in ITEMS) + "import std.tcp.Conn;\n"
    with open(os.path.join(d, "examples.vo"), "w") as f:
        f.write(imports + "\nfn main() {\n}\n\n" + "\n".join(tests))
    code, out, err = vo(d, ["test", "examples.vo"])
    failed = [l for l in out if l.startswith("FAIL") and "example of" in l]
    passed = [l for l in out if l.startswith("PASS") and "example of" in l]
    case("docs/every example holds as written", code == 0 and len(passed) == len(examples) and not failed,
         f"{code} {len(passed)}/{len(examples)} {failed} {err[-400:]}")


# ---------------------------------------------------------------- the move (SC-002, a real server)

OPENING = {"open_notes", "memory", "fresh"}  # the functions that open the database (the service's, the tests')


def allowed(line, function):
    """Whether a line may differ between the SQLite and the PostgreSQL form of a program (spec US2)."""
    t = line.strip()
    return (t == "" or t.startswith("#") or t.startswith("import ") or function in OPENING
            or "uses " in t or t.startswith("effects ") or t.startswith("export effects ")
            or "sql(" in t or t.startswith('"'))  # SQL, also a statement on lines of its own


def without_effects(line):
    """The line without its `uses` clause and effect arguments."""
    line = re.sub(r"\[uses [^\]]*\]", "", line)
    return re.sub(r" uses [^{]*\{$", " {", line)


def configuration(lines, fns):
    """Lines of constants that only the opening functions and effect sets use (where the database is)."""
    ok = set()
    for i, line in enumerate(lines):
        m = re.match(r"(?:export )?const (\w+)", line)
        if not m:
            continue
        uses = [k for k, other in enumerate(lines) if k != i and re.search(r"\b%s\b" % m.group(1), other)]
        if all(fns[k] in OPENING or "effects " in lines[k] for k in uses):
            ok.add(i)
    return ok


def functions(lines):
    """For each line, the top-level function it is in ("" outside one)."""
    out, current = [], ""
    for line in lines:
        m = re.match(r"(?:export )?fn (\w+)", line)
        if m:
            current = m.group(1)
        out.append(current)
        if line.startswith("}"):
            current = ""
    return out


def group_move(d):
    import difflib
    sqlite_dir = os.path.join(ROOT, "examples", "notes-api")
    pg_dir = os.path.join(ROOT, "tests", "postgres", "notes-api")
    for name in ("main.vo", "api.vo", "notes.vo"):
        a = open(os.path.join(sqlite_dir, name)).read().splitlines()
        b = open(os.path.join(pg_dir, name)).read().splitlines()
        fa, fb = functions(a), functions(b)
        bad = []
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if tag == "equal":
                continue
            if tag == "replace" and i2 - i1 == j2 - j1 and all(
                    without_effects(a[i1 + k]) == without_effects(b[j1 + k]) for k in range(i2 - i1)):
                continue  # only the `uses` clauses or effect arguments differ
            a_ok = configuration(a, fa)
            b_ok = configuration(b, fb)
            bad += [f"sqlite {name}:{i + 1}: {a[i]}" for i in range(i1, i2) if not allowed(a[i], fa[i]) and i not in a_ok]
            bad += [f"postgres {name}:{j + 1}: {b[j]}" for j in range(j1, j2) if not allowed(b[j], fb[j]) and j not in b_ok]
            continue
            bad += [f"sqlite {name}:{i + 1}: {a[i]}" for i in range(i1, i2) if not allowed(a[i], fa[i])]
            bad += [f"postgres {name}:{j + 1}: {b[j]}" for j in range(j1, j2) if not allowed(b[j], fb[j])]
        case(f"move/{name} differs only in imports, opening, effects and SQL (SC-002)", not bad, "\n  " + "\n  ".join(bad))
    work = os.path.join(d, "notes-api")
    shutil.copytree(pg_dir, work)
    code, out, err = vo(work, ["package", "main.vo", "postgres", "../pkg/postgres"])
    case("move/the package pins", code == 0, f"{code} {out} {err}")
    code, out, err = vo(work, ["test", "main.vo"], timeout=300)
    case("move/the PostgreSQL form passes the example's tests", code == 0 and out and out[-1].endswith(" 0 failed"),
         f"{code} {[l for l in out if l.startswith('FAIL')]} {out[-1:]} {err[-300:]}")
    code, out, err = vo(os.path.join(ROOT, "examples", "notes-api"), ["test", "main.vo"])
    case("move/the SQLite form still passes them", code == 0, f"{code} {out[-1:]}")


TIMING = """import postgres.client.Value;
import postgres.client.close;
import postgres.client.connect;
import postgres.client.exec;
import postgres.client.query;
import postgres.client.sql;
import postgres.client.transaction;
import postgres.client.Options;
import std.time.now;

fn main() -> result[void] uses console, clock, net("{HOST}:{P}"), entropy {
    let o = new Options { host: "{HOST}", port: {P}, database: "{DB}", user: "{USER}", password: "{PW}", plain_text: true, connect_ms: 10000, statement_ms: 120000 };
    let t0 = now();
    let db = try connect(o);
    let t1 = now();
    let none = new list[Value];
    let _ = try exec(db, sql("drop table if exists vo_speed"), none);
    let _ = try exec(db, sql("create table vo_speed (a int, b text, c double precision, d bigint, e text)"), none);
    let t2 = now();
    let _ = try transaction(db, fn() -> result[int] {
        for i in 0..10000 {
            let _ = try exec(db, sql("insert into vo_speed values (?, ?, ?, ?, ?)"),
                [Value.integer(i), Value.text("row " + str(i)), Value.real(1.5), Value.integer(i * 1000), Value.null]);
        }
        return ok(0);
    });
    let t3 = now();
    let rows = try query(db, sql("select i as a, 'row ' || i as b, 1.5::double precision as c, i * 1000 as d, null::text as e from generate_series(1, 100000) as i"), none);
    let t4 = now();
    let _ = try exec(db, sql("drop table vo_speed"), none);
    close(db);
    print(str(t1 - t0) + " " + str(t3 - t2) + " " + str(t4 - t3) + " " + str(len(rows)));
    return ok();
}
"""


def timing(d):
    t = real()
    src = TIMING.replace("{HOST}", t.host).replace("{P}", str(t.port)).replace("{DB}", t.db).replace("{USER}", t.user)
    src = src.replace("{PW}", t.password)
    results = []
    for _ in range(3):
        code, out, err = run(d, src, "speed.vo", extra=["--max-memory", "1024"], timeout=600)
        if code != 0 or len(out) != 1:
            print(f"FAIL timing: {code} {out} {err[-400:]}")
            return 1
        results.append([int(x) for x in out[0].split()])
    connect, insert, read = (sorted(r[k] for r in results)[1] for k in range(3))
    print(f"postgres: connect and sign in {connect} ms (SC-007: under 1000); 10,000 rows inserted in one transaction "
          f"{insert} ms (under 5000); 100,000 rows of five columns read {read} ms (under 3000); median of 3")
    return 0 if connect < 1000 and insert < 5000 and read < 3000 else 1


GROUPS = {"unit": group_unit, "connect": group_connect, "auth": group_auth, "failures": group_failures,
          "fuzz": group_fuzz, "replay": group_replay, "parity": group_parity, "docs": group_docs}
SERVER_GROUPS = {"query": group_query, "types": group_types, "hostile": group_hostile, "tx": group_tx, "move": group_move}


def main():
    args = sys.argv[1:]
    server = "--server" in args
    wanted = [args[args.index("--group") + 1]] if "--group" in args else None
    names = wanted or (list(GROUPS) + (list(SERVER_GROUPS) if server else []))
    d = project()
    if "--time" in args:
        try:
            return timing(d)
        finally:
            shutil.rmtree(d, ignore_errors=True)
    try:
        ran = 0
        for name in names:
            if name in SERVER_GROUPS and not server:
                print(f"postgres: skip {name} (needs a PostgreSQL server: --server)")
                continue
            fn = GROUPS.get(name) or SERVER_GROUPS.get(name)
            if not fn:
                print(f"unknown group {name} (known: {', '.join(list(GROUPS) + list(SERVER_GROUPS))})")
                return 2
            if name == "fuzz":
                fn(d, "--full" in args)
            else:
                fn(d)
            ran += 1
        if not server and not wanted:
            print("postgres: skip query, types, hostile, tx, move (need a PostgreSQL server: --server)")
    finally:
        shutil.rmtree(d, ignore_errors=True)
    print(f"postgres: {ran} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
