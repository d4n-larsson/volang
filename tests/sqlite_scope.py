#!/usr/bin/env python3
"""std.sqlite inside the file scopes (spec 026, research R12, T035).

A program granted file.read("data/") and file.write("data/") runs statements that make SQLite open
files of its own (`attach database`, `vacuum into`) with paths inside `data/`, absolute, with `..` and
through links, and the pragmas vo refuses or allows. Each path outside `data/` must stop the run with
R0010 at the user's call, and no file may appear (or change) outside `data/`. --quick runs a subset.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))

PROGRAM = """import std.sqlite.open;
import std.sqlite.parse_sql;
import std.sqlite.exec;
import std.sqlite.Changes;
import std.sqlite.Value;

fn main() -> result[void] uses console, file.read("data/"), file.write("data/") {
    let db = try open("data/main.db");
    match parse_sql(STMT) {
        case ok(st) {
            report(exec(db, st, PARAMS));
        }
        case err(e) {
            print("err " + e);
        }
    }
    return ok();
}

fn report(r: result[Changes]) uses console {
    match r {
        case ok(_) {
            print("ok");
        }
        case err(e) {
            print("err " + e);
        }
    }
}
"""


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def run(tmp, stmt, path=None):
    params = "[Value.text(%s)]" % vo_str(path) if path is not None else "new list[Value]"
    src = PROGRAM.replace("STMT", vo_str(stmt)).replace("PARAMS", params)
    with open(os.path.join(tmp, "prog.vo"), "w") as f:
        f.write(src)
    r = subprocess.run([VO, "run", "prog.vo"], cwd=tmp, capture_output=True, timeout=60)
    out = r.stdout.decode("utf-8", "replace").strip()
    err = r.stderr.decode("utf-8", "replace")
    if r.returncode == 70 and "R0010" in err and "prog.vo:11:" in err:
        return "R0010"
    if r.returncode == 0:
        return out
    return "exit %d: %s %s" % (r.returncode, out, err.strip().splitlines()[:1])


def outside_state(tmp):
    """Every file outside data/ (and prog.vo) with its contents."""
    state = {}
    for d, dirs, files in os.walk(tmp):
        rel = os.path.relpath(d, tmp)
        if rel == "data" or rel.startswith("data" + os.sep):
            continue
        for f in files:
            p = os.path.join(d, f)
            if os.path.islink(p) or f == "prog.vo":
                continue
            state[os.path.relpath(p, tmp)] = open(p, "rb").read()
    return state


def main():
    quick = "--quick" in sys.argv[1:]
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp = os.path.realpath(tmp)
        os.makedirs(os.path.join(tmp, "data", "sub"))
        os.makedirs(os.path.join(tmp, "secret"))
        with open(os.path.join(tmp, "secret", "s.db"), "wb") as f:
            f.write(b"not yours")
        os.symlink("../secret/s.db", os.path.join(tmp, "data", "link_out.db"))
        os.symlink("../secret", os.path.join(tmp, "data", "dir_out"))
        os.symlink("sub", os.path.join(tmp, "data", "dir_in"))
        before = outside_state(tmp)
        inside = ["data/a.db", "data/sub/b.db", tmp + "/data/c.db", "data/./d.db", "data/sub/../e.db",
                  "data/dir_in/f.db"]
        outside = ["out.db", "data/../out.db", "secret/s.db", tmp + "/secret/x.db", "/tmp/vo_scope_x.db",
                   "data/link_out.db", "data/dir_out/y.db", "data/sub/../../out.db"]
        if quick:
            inside, outside = inside[:2], outside[:3] + outside[5:6]
        cases = []
        for i, p in enumerate(inside):
            cases.append(("attach database ? as a%d" % i, p, "ok"))
            cases.append(("vacuum into ?", p.replace(".db", "_v.db"), "ok"))
        for i, p in enumerate(outside):
            cases.append(("attach database ? as o%d" % i, p, "R0010"))
            cases.append(("vacuum into ?", p.replace(".db", "_v.db") if "link" not in p else p, "R0010"))
        refused = ["journal_mode = WAL", "temp_store_directory", "data_store_directory"]
        cases += [("pragma journal_mode=wal", None, "err PRAGMA journal_mode = WAL is not available in vo"),
                  ("pragma temp_store_directory='/tmp'", None,
                   "err PRAGMA temp_store_directory is not available in vo"),
                  ("pragma data_store_directory='/tmp'", None,
                   "err PRAGMA data_store_directory is not available in vo"),
                  ("select load_extension('x')", None, "err error: no such function: load_extension")]
        for mode in ("memory", "off", "truncate", "delete"):
            cases.append(("pragma journal_mode=%s" % mode, None, "ok"))
        assert len(refused) == 3
        for stmt, path, want in cases:
            got = run(tmp, stmt, path)
            if got != want:
                bad += 1
                print("FAIL %s [%s]: got %r, want %r" % (stmt, path, got, want))
        after = outside_state(tmp)
        if after != before:
            bad += 1
            print("FAIL files outside data/ changed: %s" % sorted(set(after) ^ set(before) |
                                                                 {k for k in after if after.get(k) != before.get(k)}))
        if os.path.exists("/tmp/vo_scope_x.db") or os.path.exists("/tmp/vo_scope_x_v.db"):
            bad += 1
            print("FAIL a file appeared in /tmp")
        print("sqlite_scope: %d cases, %d failed" % (len(cases), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
