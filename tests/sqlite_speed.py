#!/usr/bin/env python3
"""Spec 026 SC-002: 100,000 inserts in one transaction into a database file under 2 s, and reading
them all back under 1 s. Figures are the median of 3 runs of `vo run`, process start included.
Needs an idle machine: not in `make test`, in `make gate`.
"""
import os
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

INSERT = """import std.sqlite.open;
import std.sqlite.sql;
import std.sqlite.exec;
import std.sqlite.transaction;
import std.sqlite.Value;

fn main() -> result[void] uses console, file.read, file.write {
    let db = try open("speed.db");
    let _ = try exec(db, sql("create table t(id integer primary key, name text, score real)"), new list[Value]);
    let n = try transaction(db, fn() -> result[int] {
        for i in 0..100000 {
            let _ = try exec(db, sql("insert into t(name, score) values (?, ?)"),
                             [Value.text("name " + str(i)), Value.real(float(i) / 7.0)]);
        }
        return ok(100000);
    });
    print(str(n));
    return ok();
}
"""

READ = """import std.sqlite.open_readonly;
import std.sqlite.sql;
import std.sqlite.query;
import std.sqlite.get_str;
import std.sqlite.Value;

fn main() -> result[void] uses console, file.read {
    let db = try open_readonly("speed.db");
    let rows = try query(db, sql("select id, name, score from t order by id"), new list[Value]);
    var total = 0;
    for row in rows {
        set total = total + len(try get_str(row, "name"));
    }
    print(str(len(rows)) + " " + str(total));
    return ok();
}
"""


def timed(tmp, name, before=None):
    times = []
    for _ in range(3):
        if before:
            before()
        t = time.monotonic()
        r = subprocess.run([VO, "run", name], cwd=tmp, capture_output=True, timeout=120)
        times.append(time.monotonic() - t)
        if r.returncode != 0:
            print("FAIL %s exited %d: %s" % (name, r.returncode, r.stderr.decode()[:1000]))
            sys.exit(1)
    return statistics.median(times), r.stdout.decode().strip()


def main():
    with tempfile.TemporaryDirectory() as tmp:
        for name, src in (("insert.vo", INSERT), ("read.vo", READ)):
            with open(os.path.join(tmp, name), "w") as f:
                f.write(src)
        db = os.path.join(tmp, "speed.db")

        def fresh():
            if os.path.exists(db):
                os.remove(db)

        ins, out1 = timed(tmp, "insert.vo", fresh)
        rd, out2 = timed(tmp, "read.vo")
    ok = ins < 2.0 and rd < 1.0 and out1 == "100000" and out2.startswith("100000 ")
    print("sqlite_speed: 100,000 inserts %.2f s (limit 2), reading them %.2f s (limit 1)%s" %
          (ins, rd, "" if ok else ": FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
