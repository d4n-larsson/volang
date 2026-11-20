#!/usr/bin/env python3
"""Spec 028: concurrent request handling with std.http.serve_workers.

Each check starts a vo server from an inline program in its own temporary directory, sends requests
(concurrently, with threads), measures each response's latency, stops it with signals and checks that
no process is left (no process has the directory as its working directory).

  serve_workers.py            every check
  serve_workers.py --quick    the short checks only (make quick)
  serve_workers.py NAME...    the named checks
"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

HEAD = """import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve;
import std.http.serve_workers;
"""


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def get(port, path, timeout=30):
    """(status, body, seconds) of GET path; status 0 when the connection closed without a response."""
    t = time.monotonic()
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
            s.sendall(("GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % path).encode())
            out = b""
            while True:
                b = s.recv(65536)
                if not b:
                    break
                out += b
    except OSError:
        out = b""
    took = time.monotonic() - t
    if not out.startswith(b"HTTP/1.1 "):
        return 0, b"", took
    head, _, body = out.partition(b"\r\n\r\n")
    return int(head[9:12]), body, took


def get_many(port, paths, timeout=30):
    """get() of every path at once, in threads; the results in the order of paths."""
    res = [None] * len(paths)

    def one(i):
        res[i] = get(port, paths[i], timeout)

    ts = [threading.Thread(target=one, args=(i,)) for i in range(len(paths))]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    return res


class Server:
    """A vo program serving on a free port ("PORT" in the program is replaced by it)."""

    def __init__(self, program, args=(), files=None, prog_args=()):
        self.dir = tempfile.TemporaryDirectory(prefix="vo-sw-")
        self.port = free_port()
        with open(os.path.join(self.dir.name, "main.vo"), "w") as f:
            f.write(program.replace("PORT", str(self.port)))
        for name, text in (files or {}).items():
            path = os.path.join(self.dir.name, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(text)
        self.out = open(os.path.join(self.dir.name, ".out"), "w+b")
        self.err = open(os.path.join(self.dir.name, ".err"), "w+b")
        self.proc = subprocess.Popen([VO, "run", *args, "main.vo", "--", *prog_args], cwd=self.dir.name, stdout=self.out,
                                     stderr=self.err)
        end = time.monotonic() + 10
        while time.monotonic() < end and self.proc.poll() is None:
            try:
                socket.create_connection(("127.0.0.1", self.port), timeout=1).close()
                return
            except OSError:
                time.sleep(0.02)
        raise RuntimeError("the server did not start: " + self.stderr())

    def signal(self, sig=signal.SIGTERM):
        self.proc.send_signal(sig)

    def wait(self, timeout=30):
        """The exit status (None when it did not end in time: it is then killed)."""
        try:
            return self.proc.wait(timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            return None

    def stdout(self):
        self.out.seek(0)
        return self.out.read().decode(errors="replace")

    def stderr(self):
        self.err.seek(0)
        return self.err.read().decode(errors="replace")

    def left_over(self):
        """Processes still working in the server's directory (workers that outlived it)."""
        n = 0
        for pid in os.listdir("/proc"):
            if pid.isdigit():
                try:
                    n += os.readlink("/proc/%s/cwd" % pid) == self.dir.name
                except OSError:
                    pass
        return n

    def close(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        self.out.close()
        self.err.close()
        self.dir.cleanup()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()


def stopped_cleanly(srv, problems):
    """SIGTERM, then: exit 0 and no process left."""
    srv.signal()
    status = srv.wait()
    if status != 0:
        problems.append("exit %s after SIGTERM; stderr: %s" % (status, srv.stderr()[-500:]))
    time.sleep(0.1)
    if srv.left_over():
        problems.append("%d processes left over" % srv.left_over())


# ---- checks: each returns a list of problems ----

ECHO = HEAD + """
fn setup() -> result[str] {
    return ok("ready");
}

fn handle(state: str, r: Request) -> Response {
    return new Response { status: 200, headers: new list[Header], body: state + " " + r.path };
}

fn main() -> result[void] uses console, serve("PORT") {
    try serve_workers("PORT", 4, setup, handle);
    return ok();
}
"""


def check_smoke():
    """4 workers answer 40 concurrent requests, each with its own path; SIGTERM ends it cleanly."""
    problems = []
    with Server(ECHO) as srv:
        paths = ["/%d" % i for i in range(40)]
        for p, (status, body, _) in zip(paths, get_many(srv.port, paths)):
            if status != 200 or body != ("ready " + p).encode():
                problems.append("%s: %s %r" % (p, status, body))
        stopped_cleanly(srv, problems)
    return problems


SLOW = HEAD + """import std.time.sleep;

fn setup() -> result[int] {
    return ok(0);
}

fn route(r: Request) -> Response uses clock {
    if r.path == "/slow" {
        sleep(2000);
    }
    return new Response { status: 200, headers: new list[Header], body: r.path };
}

fn main() -> result[void] uses clock, serve("PORT") {
    if len(args()) > 0 {
        return serve("PORT", route);
    }
    return serve_workers("PORT", 4, setup, fn(n: int, r: Request) -> Response { return route(r); });
}
"""


def slow_then_fast(srv, fast):
    """3 slow requests in progress, then `fast` fast ones one after another: their latencies."""
    slow = threading.Thread(target=get_many, args=(srv.port, ["/slow"] * 3))
    slow.start()
    time.sleep(0.2)
    took = [get(srv.port, "/fast")[2] for _ in range(fast)]
    slow.join()
    return took


def check_slow_vs_fast():
    """SC-001: with 4 workers and 3 slow requests in progress, 50 fast requests each within 50 ms; with
    serve (today's behavior) the first fast one waits for the slow ones."""
    problems = []
    with Server(SLOW) as srv:
        took = slow_then_fast(srv, 50)
        print("    serve_workers: slowest of 50 fast responses %.1f ms" % (max(took) * 1000))
        if max(took) > 0.05:
            problems.append("serve_workers: slowest fast response %.0f ms (want <= 50)" % (max(took) * 1000))
        stopped_cleanly(srv, problems)
    with Server(SLOW, prog_args=("serve",)) as srv:
        took = slow_then_fast(srv, 1)
        if took[0] < 1.0:
            problems.append("serve: a fast request was answered in %.0f ms during slow ones" % (took[0] * 1000))
        stopped_cleanly(srv, problems)
    return problems


FAILING = HEAD + """import std.time.sleep;

fn setup() -> result[int] {
    return ok(0);
}

fn handle(zero: int, r: Request) -> Response uses clock {
    if r.path == "/crash" {
        return new Response { status: 200, headers: new list[Header], body: str(1 / zero) };
    }
    if r.path == "/hang" {
        sleep(10000);
    }
    if r.path == "/mem" {
        var s = "x";
        while true {
            set s = s + s;
        }
    }
    if r.path == "/block" {
        sleep(3000);
    }
    return new Response { status: 200, headers: new list[Header], body: r.path };
}

fn main() -> result[void] uses clock, serve("PORT") {
    return serve_workers("PORT", 4, setup, handle);
}
"""


def workers_of(pid):
    """The pids of a process's children."""
    kids = []
    for p in os.listdir("/proc"):
        if p.isdigit():
            try:
                stat = open("/proc/%s/stat" % p).read()
            except OSError:
                continue
            if int(stat.rsplit(")", 1)[1].split()[1]) == pid:
                kids.append(int(p))
    return kids


def check_isolation():
    """SC-003: among 20 requests in flight, a crashing, a timing-out and a memory-exhausting one get 500,
    the other 17 and the next 100 are answered; a worker killed from outside while handling a request:
    500 (or the connection closes), a note, and 100 more requests answered."""
    problems = []
    with Server(FAILING, args=("--timeout", "1", "--max-memory", "64")) as srv:
        paths = ["/crash", "/hang", "/mem"] + ["/ok%d" % i for i in range(17)]
        res = get_many(srv.port, paths)
        for p, (status, body, _) in zip(paths, res):
            want = 500 if p in ("/crash", "/hang", "/mem") else 200
            if status != want:
                problems.append("%s: %s (want %s)" % (p, status, want))
        paths = ["/next%d" % i for i in range(100)]
        bad = [p for p, r in zip(paths, get_many(srv.port, paths)) if r[0] != 200]
        if bad:
            problems.append("%d of the next 100 not answered: %s" % (len(bad), bad[:3]))
        err = srv.stderr()
        for code in ("R0002", "R0009", "R0006"):
            if code not in err:
                problems.append("no %s report on stderr" % code)
        before = set(workers_of(srv.proc.pid))
        blocked = []
        t = threading.Thread(target=lambda: blocked.append(get(srv.port, "/block")))
        t.start()
        time.sleep(0.3)
        busy = [w for w in before if "pipe_read" not in open("/proc/%d/wchan" % w).read()]
        if len(busy) != 1:
            problems.append("cannot tell the busy worker: %d of %d" % (len(busy), len(before)))
        else:
            os.kill(busy[0], signal.SIGKILL)
        t.join()
        if blocked[0][0] not in (0, 500):
            problems.append("killed worker's request: %s (want 500 or closed)" % blocked[0][0])
        paths = ["/after%d" % i for i in range(100)]
        bad = [p for p, r in zip(paths, get_many(srv.port, paths)) if r[0] != 200]
        if bad:
            problems.append("%d of 100 after the kill not answered" % len(bad))
        if "ended unexpectedly; it was replaced" not in srv.stderr():
            problems.append("no note about the killed worker")
        if len(workers_of(srv.proc.pid)) != 4:
            problems.append("%d workers after the kill (want 4)" % len(workers_of(srv.proc.pid)))
        stopped_cleanly(srv, problems)
    return problems


SLEEPER = HEAD + """import std.time.sleep;

fn setup() -> result[int] {
    return ok(0);
}

fn handle(n: int, r: Request) -> Response uses clock {
    match parse_int(slice(r.path, 1, len(r.path))) {
        case ok(ms) {
            sleep(ms);
        }
        case err(_) {
        }
    }
    return new Response { status: 200, headers: new list[Header], body: r.path };
}

fn main() -> result[void] uses console, clock, serve("PORT") {
    try serve_workers("PORT", 4, setup, handle);
    print("stopped");
    return ok();
}
"""


def in_background(port, paths):
    """get_many(paths) in a thread; join() it, then read .result."""
    t = threading.Thread(target=lambda: setattr(t, "result", get_many(port, paths)))
    t.start()
    time.sleep(0.2)
    return t


def check_shutdown():
    """SC-004: 5 slow requests in progress (4 workers), SIGTERM: all 5 answered, new connections
    refused, exit 0. A second SIGINT during shutdown stops at once. A request still running at its
    --timeout is stopped with 500 and shutdown completes."""
    problems = []
    with Server(SLEEPER) as srv:
        t = in_background(srv.port, ["/1000"] * 5)
        srv.signal()
        time.sleep(0.1)
        if get(srv.port, "/0", timeout=2)[0] != 0:
            problems.append("a new connection was answered during shutdown")
        t.join()
        if [r[0] for r in t.result] != [200] * 5:
            problems.append("in-progress requests: %s (want five 200)" % [r[0] for r in t.result])
        if srv.wait() != 0 or "stopped" not in srv.stdout():
            problems.append("SIGTERM: exit %s, stdout %r" % (srv.proc.returncode, srv.stdout()))
        if srv.left_over():
            problems.append("SIGTERM: %d processes left over" % srv.left_over())
    with Server(SLEEPER) as srv:
        t = in_background(srv.port, ["/5000"] * 4)
        started = time.monotonic()
        srv.signal(signal.SIGINT)
        time.sleep(0.2)
        srv.signal(signal.SIGINT)
        status = srv.wait(5)
        took = time.monotonic() - started
        t.join()
        if status != 0 or took > 1.5:
            problems.append("second SIGINT: exit %s after %.1f s (want 0, at once)" % (status, took))
        time.sleep(0.1)
        if srv.left_over():
            problems.append("second SIGINT: %d processes left over" % srv.left_over())
    with Server(SLEEPER, args=("--timeout", "1")) as srv:
        t = in_background(srv.port, ["/10000"])
        started = time.monotonic()
        srv.signal()
        status = srv.wait(10)
        took = time.monotonic() - started
        t.join()
        if t.result[0][0] != 500 or status != 0 or took > 3:
            problems.append("--timeout during shutdown: %s, exit %s after %.1f s (want 500, 0, about 1 s)"
                            % (t.result[0][0], status, took))
    return problems


COUNTER = HEAD + """import std.sqlite.Database;
import std.sqlite.Value;
import std.sqlite.exec;
import std.sqlite.get_int;
import std.sqlite.open;
import std.sqlite.query;
import std.sqlite.sql;
import std.sqlite.transaction;

fn setup() -> result[Database[uses file.read, file.write]] uses file.read("data/"), file.write("data/") {
    let db = try open("data/count.db");
    let _ = try exec(db, sql("insert into opens default values"), new list[Value]);
    return ok(db);
}

fn handle(db: Database[uses file.read, file.write], r: Request) -> Response uses file.read("data/"), file.write("data/") {
    let next = transaction(db, fn() -> result[int] {
        let rows = try query(db, sql("select n from counter"), new list[Value]);
        let n = try get_int(rows[0], "n") + 1;
        let _ = try exec(db, sql("update counter set n = ?"), [Value.integer(n)]);
        return ok(n);
    });
    match next {
        case ok(n) {
            return new Response { status: 200, headers: new list[Header], body: str(n) };
        }
        case err(e) {
            return new Response { status: 500, headers: new list[Header], body: e };
        }
    }
}

fn main() -> result[void] uses file.read("data/"), file.write("data/"), serve("PORT") {
    let db = try open("data/count.db");
    let _ = try exec(db, sql("create table counter (n integer)"), new list[Value]);
    let _ = try exec(db, sql("insert into counter values (0)"), new list[Value]);
    let _ = try exec(db, sql("create table opens (id integer primary key)"), new list[Value]);
    return serve_workers("PORT", 4, setup, handle);
}
"""


def check_counter():
    """SC-005: 1,000 concurrent requests increment a SQLite counter in transactions through 4 workers:
    the counter is 1,000 and the database was opened once per worker."""
    import concurrent.futures
    import sqlite3
    problems = []
    with Server(COUNTER, files={"data/.keep": ""}) as srv:
        with concurrent.futures.ThreadPoolExecutor(32) as pool:
            res = list(pool.map(lambda i: get(srv.port, "/"), range(1000)))
        bad = [r for r in res if r[0] != 200]
        if bad:
            problems.append("%d of 1000 not 200, e.g. %s %r" % (len(bad), bad[0][0], bad[0][1][:200]))
        stopped_cleanly(srv, problems)
        db = sqlite3.connect(os.path.join(srv.dir.name, "data", "count.db"))
        n = db.execute("select n from counter").fetchone()[0]
        opens = db.execute("select count(*) from opens").fetchone()[0]
        db.close()
        if n != 1000 or opens != 4:
            problems.append("counter %d (want 1000), opens %d (want 4)" % (n, opens))
    return problems


FRAGILE = HEAD + """import std.time.sleep;

fn setup() -> result[int] uses file.read("data/") {
    match read_file("data/broken") {
        case ok(_) {
            return err("the database is broken");
        }
        case err(_) {
            return ok(0);
        }
    }
}

fn handle(n: int, r: Request) -> Response uses clock {
    if r.path == "/block" {
        sleep(3000);
    }
    return new Response { status: 200, headers: new list[Header], body: r.path };
}

fn main() -> result[void] uses console, clock, file.read("data/"), serve("PORT") {
    try serve_workers("PORT", 2, setup, handle);
    print("not reached");
    return ok();
}
"""


def check_replacement_setup():
    """R5: a replacement worker whose setup returns err shuts the server down as on SIGTERM (the other
    requests already read are answered) and serve_workers returns the err."""
    problems = []
    with Server(FRAGILE, files={"data/.keep": ""}) as srv:
        t = in_background(srv.port, ["/block"])
        other = in_background(srv.port, ["/block"])
        open(os.path.join(srv.dir.name, "data", "broken"), "w").close()
        busy = [w for w in workers_of(srv.proc.pid) if "pipe_read" not in open("/proc/%d/wchan" % w).read()]
        os.kill(busy[0], signal.SIGKILL)
        status = srv.wait(10)
        t.join()
        other.join()
        codes = sorted([t.result[0][0], other.result[0][0]])
        if codes not in ([0, 200], [200, 500]):
            problems.append("requests: %s (want the killed one 500 or closed, the other 200)" % codes)
        err = srv.stderr()
        if status != 1 or "error: the database is broken" not in err or "not reached" in srv.stdout():
            problems.append("exit %s, stderr %r" % (status, err[-300:]))
        time.sleep(0.1)
        if srv.left_over():
            problems.append("%d processes left over" % srv.left_over())
    return problems


def check_replay():
    """SC-006: a recorded 200-request session against 4 workers reading and writing a database replays to
    the same responses 20 times, with the database gone (no files touched)."""
    import concurrent.futures
    problems = []
    program = open(os.path.join(ROOT, "tests", "cases", "serve_workers_steps", "replay", "main.vo")).read()
    program = program.replace("{vport}", "PORT").replace('serve_workers("PORT", 2,', 'serve_workers("PORT", 4,')
    with Server(program, args=("--record", "r.jsonl"), files={"data/.keep": ""}) as srv:
        with concurrent.futures.ThreadPoolExecutor(16) as pool:
            res = list(pool.map(lambda i: get(srv.port, "/count" if i % 4 == 3 else "/add"), range(200)))
        if any(r[0] != 200 for r in res):
            problems.append("recording: %d responses not 200" % sum(r[0] != 200 for r in res))
        stopped_cleanly(srv, problems)
        d = srv.dir.name
        for name in os.listdir(os.path.join(d, "data")):
            os.remove(os.path.join(d, "data", name))
        os.rmdir(os.path.join(d, "data"))
        outs = set()
        for _ in range(20):
            r = subprocess.run([VO, "replay", "r.jsonl"], cwd=d, capture_output=True)
            outs.add((r.returncode, r.stdout, r.stderr))
        if len(outs) != 1 or next(iter(outs)) != (0, b"", b""):
            problems.append("replays: %d different outcomes, e.g. %r" % (len(outs), next(iter(outs))[:3]))
        if os.path.exists(os.path.join(d, "data")):
            problems.append("replay touched data/")
    return problems


def post(port, path, body):
    """The status of POST path with body."""
    data = ("POST %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: %d\r\n\r\n%s" % (path, len(body), body)).encode()
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=30) as s:
            s.sendall(data)
            out = s.recv(64)
    except OSError:
        return 0
    return int(out[9:12]) if out.startswith(b"HTTP/1.1 ") else 0


def check_notes_load():
    """SC-005 with the example: notes-api under 1,000 concurrent POSTs loses no note."""
    import concurrent.futures
    import json
    problems = []
    ex = os.path.join(ROOT, "examples", "notes-api")
    program = open(os.path.join(ex, "main.vo")).read().replace("8080", "PORT")
    files = {n: open(os.path.join(ex, n)).read() for n in ("api.vo", "notes.vo")}
    files["data/.keep"] = ""
    with Server(program, files=files) as srv:
        with concurrent.futures.ThreadPoolExecutor(32) as pool:
            codes = list(pool.map(lambda i: post(srv.port, "/notes", '{"text": "note %d"}' % i), range(1000)))
        if codes.count(201) != 1000:
            problems.append("%d of 1000 POSTs not 201, e.g. %s" % (1000 - codes.count(201), [c for c in codes if c != 201][:3]))
        status, body, _ = get(srv.port, "/notes")
        notes = json.loads(body) if status == 200 else []
        if len(notes) != 1000 or len({n["text"] for n in notes}) != 1000:
            problems.append("GET /notes: %s, %d notes (want 1000 different)" % (status, len(notes)))
        stopped_cleanly(srv, problems)
    return problems


CHECKS = [  # (name, function, in --quick)
    ("smoke", check_smoke, True),
    ("slow_vs_fast", check_slow_vs_fast, False),  # timing: not under the load of make test
    ("isolation", check_isolation, True),
    ("shutdown", check_shutdown, True),
    ("counter", check_counter, False),
    ("replacement_setup", check_replacement_setup, True),
    ("replay", check_replay, True),
    ("notes_load", check_notes_load, False),
]


def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    names = [a for a in args if not a.startswith("--")]
    failed = 0
    for name, fn, short in CHECKS:
        if names and name not in names or quick and not short:
            continue
        t = time.monotonic()
        problems = fn()
        print("%s %s (%.1f s)" % ("FAIL" if problems else "ok  ", name, time.monotonic() - t))
        for p in problems:
            print("    " + p)
        failed += bool(problems)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
