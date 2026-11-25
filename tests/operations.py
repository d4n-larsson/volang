#!/usr/bin/env python3
"""Spec 055: operating a service, end to end, on real processes.

  workers  examples/notes-api under `vo run --workers 4`: N requests across routes; /metrics read with a
           strict parser of the text format 0.0.4 (written here: no scraper is installed in CI); the totals
           of http_requests_total equal the requests sent (SC-003); then M requests to distinct /notes/N
           paths add no series (SC-004)
  logs     the same run: every stderr line is a JSON record; every `request` and `note created` record has
           a request_id (SC-002); the last is the `stopped` record
  secrets  a service logging its requests' headers and JSON bodies: Authorization, Cookie and a password
           field never reach stderr (SC-005)
  drain    --drain: a request running at the limit is cut off with 503 within the limit plus 1 s, and the
           stop record says so (SC-006); one finishing in time is answered; the default 30 is in force; a
           second signal stops at once; plain `serve` checks the limit between requests

  operations.py              every group (N 1,000, M 10,000)
  operations.py --quick      N 200, M 1,000 (make quick)
  operations.py GROUP...     the named groups
"""
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from migrate_058 import module_form  # noqa: E402  spec 058: programs assembled from fragments

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
QUICK = "--quick" in sys.argv
N = 200 if QUICK else 1000
M = 1000 if QUICK else 10000
problems = []


def fail(group, what):
    problems.append(f"{group}: {what}")
    print(f"FAIL {group}: {what}")


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def request_bytes(method, path, body="", headers=()):
    head = f"{method} {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n"
    for k, v in headers:
        head += f"{k}: {v}\r\n"
    data = body.encode()
    if body:
        head += f"Content-Type: application/json\r\nContent-Length: {len(data)}\r\n"
    return head.encode() + b"\r\n" + data


def http(port, method, path, body="", headers=(), timeout=30, sock=None):
    """(status, body text) of one request on its own connection; status 0 when no response came. sock: a
    connection the request was already sent on."""
    try:
        with sock or socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
            if not sock:
                s.sendall(request_bytes(method, path, body, headers))
            out = b""
            while True:
                b = s.recv(65536)
                if not b:
                    break
                out += b
    except OSError:
        return 0, ""
    if not out.startswith(b"HTTP/1.1 "):
        return 0, ""
    return int(out[9:12]), out.partition(b"\r\n\r\n")[2].decode(errors="replace")


def wait_port(port, proc, seconds=30):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if proc.poll() is not None:
            return False
        try:
            socket.create_connection(("127.0.0.1", port), timeout=1).close()
            return True
        except OSError:
            time.sleep(0.05)
    return False


class Server:
    """`vo run ARGS` in workdir, its stdout and stderr in files there (a pipe left unread fills up and blocks
    the server once it has written 64 KiB of records)."""

    def __init__(self, workdir, args):
        self.out = open(os.path.join(workdir, "stdout"), "w+b")
        self.err = open(os.path.join(workdir, "stderr"), "w+b")
        self.p = subprocess.Popen([VO, "run"] + args, cwd=workdir, stdout=self.out, stderr=self.err)
        self.pid = self.p.pid

    def poll(self):
        return self.p.poll()

    def send_signal(self, s):
        self.p.send_signal(s)

    def kill(self):
        self.p.kill()

    @property
    def returncode(self):
        return self.p.returncode

    def communicate(self, timeout=None):
        """(stdout, stderr) bytes once it ends; raises subprocess.TimeoutExpired."""
        self.p.wait(timeout=timeout)
        self.out.seek(0)
        self.err.seek(0)
        return self.out.read(), self.err.read()

    def stderr_text(self):
        self.err.seek(0)
        return self.err.read().decode(errors="replace")


def start(workdir, args):
    return Server(workdir, args)


def in_threads(fn, items, threads=16):
    """fn(item) for every item, in threads; the results in the order of items."""
    out = [None] * len(items)
    nxt = [0]
    lock = threading.Lock()

    def run():
        while True:
            with lock:
                i = nxt[0]
                nxt[0] += 1
            if i >= len(items):
                return
            out[i] = fn(items[i])
    ts = [threading.Thread(target=run) for _ in range(threads)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    return out


# ---- the text exposition format 0.0.4, strictly ----

NAME = r"[a-zA-Z_:][a-zA-Z0-9_:]*"
LABEL = r"[a-zA-Z_][a-zA-Z0-9_]*"


def parse_metrics(text):
    """{(name, ((label, value), ...)): float} of every sample, or raises ValueError naming the line."""
    if not text.endswith("\n"):
        raise ValueError("the text does not end with a line break")
    types, samples, seen_samples = {}, {}, set()
    for n, line in enumerate(text[:-1].split("\n"), 1):
        if line.startswith("#"):
            m = re.fullmatch(r"# (HELP|TYPE) (" + NAME + r") (.*)", line)
            if not m:
                if line.startswith("# HELP") or line.startswith("# TYPE"):
                    raise ValueError(f"line {n}: bad {line[2:6]} line: {line!r}")
                continue
            if m.group(1) == "TYPE":
                if m.group(3) not in ("counter", "gauge", "histogram", "summary", "untyped"):
                    raise ValueError(f"line {n}: unknown type {m.group(3)!r}")
                if m.group(2) in types or m.group(2) in seen_samples:
                    raise ValueError(f"line {n}: TYPE of {m.group(2)} repeated or after its samples")
                types[m.group(2)] = m.group(3)
            elif re.search(r"\\[^\\n]", m.group(3).replace("\\\\", "")):
                raise ValueError(f"line {n}: HELP with a bad escape")
            continue
        m = re.match(r"(" + NAME + r")", line)
        if not m:
            raise ValueError(f"line {n}: no metric name: {line!r}")
        name, rest, labels = m.group(1), line[m.end():], []
        if rest.startswith("{"):
            i = 1
            while rest[i] != "}":
                lm = re.match(r"(" + LABEL + r")=\"", rest[i:])
                if not lm:
                    raise ValueError(f"line {n}: bad label at {rest[i:]!r}")
                i += lm.end()
                value = ""
                while rest[i] != '"':
                    if rest[i] == "\\":
                        if rest[i + 1] not in '\\"n':
                            raise ValueError(f"line {n}: bad escape in a label value")
                        value += {"\\": "\\", '"': '"', "n": "\n"}[rest[i + 1]]
                        i += 2
                    else:
                        value += rest[i]
                        i += 1
                i += 1
                labels.append((lm.group(1), value))
                if rest[i] == ",":
                    i += 1
                elif rest[i] != "}":
                    raise ValueError(f"line {n}: labels not separated by commas")
            rest = rest[i + 1:]
        parts = rest.split(" ")
        if len(parts) not in (2, 3) or parts[0] != "":
            raise ValueError(f"line {n}: not `name value`: {line!r}")
        v = parts[1]
        if not (re.fullmatch(r"[-+]?(\d+\.?\d*([eE][-+]?\d+)?|\.\d+([eE][-+]?\d+)?)", v) or v in ("+Inf", "-Inf", "NaN")):
            raise ValueError(f"line {n}: bad value {v!r}")
        base = re.sub(r"_(bucket|sum|count)$", "", name)
        seen_samples.add(base if types.get(base) == "histogram" else name)
        key = (name, tuple(labels))
        if key in samples:
            raise ValueError(f"line {n}: sample repeated")
        samples[key] = float(v)
    for base, t in types.items():  # histograms: buckets increasing to +Inf, cumulative, _count = +Inf bucket
        if t != "histogram":
            continue
        series = {}
        for (name, labels), v in samples.items():
            if name == base + "_bucket":
                rest = tuple(kv for kv in labels if kv[0] != "le")
                le = dict(labels).get("le")
                series.setdefault(rest, []).append((float(le), v))
        for rest, bs in series.items():
            if bs[-1][0] != float("inf") or [b for b, _ in bs] != sorted(b for b, _ in bs):
                raise ValueError(f"{base}{dict(rest)}: buckets not increasing to +Inf")
            if any(bs[i][1] > bs[i + 1][1] for i in range(len(bs) - 1)):
                raise ValueError(f"{base}{dict(rest)}: bucket counts not cumulative")
            if samples.get((base + "_count", rest)) != bs[-1][1] or (base + "_sum", rest) not in samples:
                raise ValueError(f"{base}{dict(rest)}: _count or _sum wrong or missing")
    return samples


def check_parser():
    """The parser refuses what it must (a strict parser that accepts anything proves nothing)."""
    good = "# HELP a_total A.\n# TYPE a_total counter\na_total{x=\"q\\\"\"} 1\n"
    parse_metrics(good)
    for bad in [good[:-1], good.replace("a_total{", "a-total{"), good.replace('x="', 'x=q"'),
                good.replace("} 1", "} one"), good.replace('\\"', "\\t")]:
        try:
            parse_metrics(bad)
        except ValueError:
            continue
        fail("workers", f"the format parser accepts {bad!r}")


# ---- workers and logs: the example ----

def example_copy(port):
    d = tempfile.mkdtemp(prefix="vo-ops-")
    src = os.path.join(ROOT, "examples", "notes-api")
    for f in ("main.vo", "api.vo", "notes.vo", "vo.effects"):
        shutil.copy(os.path.join(src, f), d)
    os.mkdir(os.path.join(d, "data"))
    for f in ("main.vo", "vo.effects"):
        p = os.path.join(d, f)
        text = open(p).read().replace("8080", str(port))
        open(p, "w").write(module_form(text))
    return d


def group_workers_and_logs(groups):
    port = free_port()
    d = example_copy(port)
    p = start(d, ["--workers", "4", "main.vo"])
    try:
        if not wait_port(port, p):
            fail("workers", "the example did not start: " + p.stderr_text()[-2000:])
            return
        sent = []  # (method, path)
        for i in range(10):
            sent.append(("POST", "/notes"))
        for i in range(N - 10 - N // 10):
            sent.append(("GET", "/notes/%d" % (i % 10 + 1)))
        for i in range(N // 10):
            sent.append(("GET", "/missing/%d" % i if i % 2 else "/notes/99999"))
        statuses = in_threads(lambda r: http(port, r[0], r[1], '{"text": "n"}' if r[0] == "POST" else "")[0], sent)
        if 0 in statuses:
            fail("workers", f"{statuses.count(0)} requests got no response")
        status, text = http(port, "GET", "/metrics")
        try:
            samples = parse_metrics(text)
        except ValueError as e:
            fail("workers", f"/metrics is not valid text format 0.0.4: {e}")
            samples = {}
        total = sum(v for (name, _), v in samples.items() if name == "http_requests_total")
        if total != len(sent):
            fail("workers", f"SC-003: http_requests_total sums to {total}, {len(sent)} requests were sent")
        else:
            print(f"workers: {len(sent)} requests over 4 workers, /metrics totals {int(total)} (SC-003)")
        _, text = http(port, "GET", "/metrics")  # (with the first scrape's own series)
        before = {k for k in parse_metrics(text) if k[0] != "http_requests_in_flight"}
        in_threads(lambda i: http(port, "GET", "/notes/%d" % (100000 + i)), list(range(M)))
        _, text = http(port, "GET", "/metrics")
        after = {k for k in parse_metrics(text) if k[0] != "http_requests_in_flight"}
        new = {k[0] + str(dict(k[1])) for k in after - before}
        if new:
            fail("workers", f"SC-004: {M} distinct paths added series: {sorted(new)[:3]}")
        else:
            print(f"workers: {M} distinct paths, no new series (SC-004)")
    finally:
        p.send_signal(signal.SIGTERM)
        try:
            out, err = p.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            p.kill()
            out, err = p.communicate()
            fail("workers", "the example did not stop within 60 s of SIGTERM")
        shutil.rmtree(d, ignore_errors=True)
    if "logs" not in groups:
        return
    lines = err.decode().splitlines()
    records, bad = [], []
    for line in lines:
        try:
            records.append(json.loads(line))
        except ValueError:
            bad.append(line)
    if bad:
        fail("logs", f"SC-002: {len(bad)} stderr lines are not JSON, the first: {bad[0][:200]!r}")
    reqs = [r for r in records if r.get("message") in ("request", "note created")]
    no_id = [r for r in reqs if not r.get("request_id")]
    n_requests = sum(r.get("message") == "request" for r in records)
    if no_id:
        fail("logs", f"SC-002: {len(no_id)} request or note records without request_id, e.g. {no_id[0]}")
    if n_requests < len(sent) + M:
        fail("logs", f"{n_requests} request records for {len(sent) + M} requests (and the scrapes)")
    if not records or records[-1].get("message") != "stopped":
        fail("logs", f"the last stderr record is not the stop record: {records[-1:]}")
    if not bad and not no_id:
        print(f"logs: {len(records)} records, all JSON, every request record with its id (SC-002)")


# ---- secrets ----

SECRETS_APP = """import std.http;
import std.json;
import std.log;
import std.web;

fn seen(n: int, r: http.Request, next: fn(http.Request) -> http.Response uses log, clock) -> http.Response uses log, clock {
    let fields = [log.field("headers", r.headers)];
    match json.parse_json(r.body) {
        case ok(j) {
            push(fields, log.field("body", j));
        }
        case err(_) {
        }
    }
    log.log_info("received", fields);
    return next(r);
}

record Login {
    user: str;
    password: str;
}

record LoginIn {
    body: Login;
}

fn login(n: int, input: LoginIn) -> result[http.Response] {
    return ok(web.json_reply(200, "welcome"));
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses serve("PORT"), log, clock {
    let app = new web.App[int, uses log, clock] {
        routes: [web.on_post("/login", login)],
        steps: [web.request_logs(), new web.Step[int, uses log, clock] { prefix: "/", run: seen }],
        errors: new list[web.ErrorStatus]
    };
    return web.serve_app("PORT", 2, setup, app);
}
"""


def group_secrets():
    port = free_port()
    d = tempfile.mkdtemp(prefix="vo-ops-")
    open(os.path.join(d, "main.vo"), "w").write(module_form(SECRETS_APP.replace("PORT", str(port))))
    p = start(d, ["main.vo"])
    try:
        if not wait_port(port, p):
            fail("secrets", "the app did not start: " + p.stderr_text()[-2000:])
            return
        values = ["Bearer tok-4af9e1", "sid=c00k1e-77", "Basic cHJveHk6c2VjcmV0", "hunter2-pw"]
        status, _ = http(port, "POST", "/login", '{"user": "ada", "password": "hunter2-pw"}',
                         [("Authorization", values[0]), ("Cookie", values[1]), ("Proxy-Authorization", values[2])])
        if status != 200:
            fail("secrets", f"POST /login answered {status}")
    finally:
        p.send_signal(signal.SIGTERM)
        out, err = p.communicate(timeout=60)
        shutil.rmtree(d, ignore_errors=True)
    text = out.decode() + err.decode()
    leaked = [v for v in values if v in text]
    if leaked:
        fail("secrets", f"SC-005: secret values in the output: {leaked}")
    elif "[redacted]" not in text or '"user":"ada"' not in text:
        fail("secrets", "the logged request is missing: " + text[-500:])
    else:
        print("secrets: Authorization, Cookie, Proxy-Authorization and a password field never written (SC-005)")


# ---- drain ----

DRAIN_APP = """import std.http;

fn busy(ms: int) uses clock {
    let end = now_ms() + ms;
    while now_ms() < end {
    }
}

fn handle(n: int, r: http.Request) -> http.Response uses clock {
    if r.path == "/slow" {
        busy(10000);
    } else if r.path == "/fast" {
        busy(1500);
    }
    return new http.Response { status: 200, headers: new list[http.Header], body: r.path };
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses serve("PORT"), clock {
    return http.serve_workers("PORT", 2, setup, handle);
}
"""

PLAIN_APP = """import std.http;

fn handle(r: http.Request) -> http.Response uses clock {
    let end = now_ms() + 2000;
    while now_ms() < end {
    }
    return new http.Response { status: 200, headers: new list[http.Header], body: r.path };
}

fn main() -> result[void] uses serve("PORT"), clock {
    return http.serve("PORT", handle);
}
"""


def drain_case(name, app, args, paths, signals, want_statuses, want_record, within, gap=0.05):
    """Starts app, sends paths at once (in threads), sends the signals (SIGTERM) after 0.5 s (0.2 s apart),
    checks the responses, the exit (code 0 within `within` s of the first signal) and the stop record.
    gap 0: the requests are sent while the server is stopped (SIGSTOP), so it reads them all in one round."""
    port = free_port()
    d = tempfile.mkdtemp(prefix="vo-ops-")
    open(os.path.join(d, "main.vo"), "w").write(module_form(app.replace("PORT", str(port))))
    p = start(d, args + ["main.vo"])
    try:
        if not wait_port(port, p):
            fail("drain", f"{name}: the app did not start: " + p.stderr_text()[-2000:])
            return
        results = [None] * len(paths)
        socks = [None] * len(paths)
        if not gap:  # connected (and accepted) first, then sent in one burst: read in one round
            socks = [socket.create_connection(("127.0.0.1", port), timeout=60) for _ in paths]
            time.sleep(0.3)
            p.send_signal(signal.SIGSTOP)
            for _ in range(200):  # (the signal is sent, not yet taken: wait until it is stopped)
                with open(f"/proc/{p.pid}/stat") as f:
                    if f.read().rsplit(")", 1)[1].split()[0] == "T":
                        break
                time.sleep(0.01)
            for sk, path in zip(socks, paths):
                sk.sendall(request_bytes("GET", path))
            p.send_signal(signal.SIGCONT)

        def one(i):
            results[i] = http(port, "GET", paths[i], timeout=60, sock=socks[i])[0]
        ts = [threading.Thread(target=one, args=(i,)) for i in range(len(paths))]
        for t in ts:
            t.start()
            time.sleep(gap)
        time.sleep(0.5)
        t0 = time.monotonic()
        for k in range(signals):
            if k:
                time.sleep(0.2)
            p.send_signal(signal.SIGTERM)
        try:
            out, err = p.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            p.kill()
            out, err = p.communicate()
        took = time.monotonic() - t0
        for t in ts:
            t.join()
    finally:
        if p.poll() is None:
            p.kill()
        shutil.rmtree(d, ignore_errors=True)
    lines = err.decode().strip().splitlines()
    try:
        record = json.loads(lines[-1])
    except (ValueError, IndexError):
        record = None
    got = {k: record.get(k) for k in want_record} if record else None
    ok = True
    if sorted(results) != sorted(want_statuses):
        fail("drain", f"{name}: responses {results}, expected {want_statuses}")
        ok = False
    if p.returncode != 0 or took > within:
        fail("drain", f"{name}: exit {p.returncode} after {took:.1f} s (expected 0 within {within} s)")
        ok = False
    if got != want_record:
        fail("drain", f"{name}: stop record {lines[-1:] if lines else None}, expected {want_record}")
        ok = False
    if ok:
        print(f"drain: {name}: {results}, stopped after {took:.1f} s, {json.dumps(got)}")


def group_drain():
    drain_case("cut off at the limit (SC-006)", DRAIN_APP, ["--drain", "2"], ["/slow"], 1, [503],
               {"level": "warn", "message": "stopped", "drain_s": 2, "drained": 0, "cut_off": 1}, 3.0)
    drain_case("answered in time", DRAIN_APP, ["--drain", "2"], ["/fast"], 1, [200],
               {"level": "info", "drain_s": 2, "drained": 1, "cut_off": 0}, 3.0)
    drain_case("the default limit", DRAIN_APP, [], ["/fast"], 1, [200], {"drain_s": 30, "cut_off": 0}, 3.0)
    drain_case("a second signal stops at once", DRAIN_APP, [], ["/slow"], 2, [0],
               {"drain_s": 30, "cut_off": 1}, 2.0)
    # the signal is seen after /a (2 s); /b is taken before the limit (1 s later) and runs past it; /c is cut off
    drain_case("plain serve: the limit between requests", PLAIN_APP, ["--drain", "1"], ["/a", "/b", "/c"], 1,
               [200, 200, 503], {"drain_s": 1, "drained": 1, "cut_off": 1}, 5.0, gap=0)


def main():
    names = [a for a in sys.argv[1:] if not a.startswith("--")] or ["workers", "logs", "secrets", "drain"]
    if "workers" in names or "logs" in names:
        check_parser()
        group_workers_and_logs(names)
    if "secrets" in names:
        group_secrets()
    if "drain" in names:
        group_drain()
    print(f"operations: {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
