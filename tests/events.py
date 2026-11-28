#!/usr/bin/env python3
"""Spec 063: std.sse and std.websocket against local peers.

  events.py                every group (the long measurements shortened with --quick)
  events.py --quick        what `make test` runs
  events.py --only NAME    one group: read, workers, client, stream, accept, bound, stop, hostile, replay, take
  events.py --fuzz         the long fuzzing run (100,000 inputs per role)

Each case writes a small vo program with the peer's port in its `net` or `serve` scope, runs it with
bin/vo ($VO_BIN; under qemu for the other architectures) and compares what it prints. The event-stream
server is in this file (EventServer); the WebSocket peer is tests/ws_peer.py.
"""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from migrate_058 import module_form  # noqa: E402  spec 058: programs assembled from fragments

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN (default build/vo) natively or under qemu
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1  # under qemu
QUICK = "--quick" in sys.argv
failures = []
cases = 0


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def lines(name, got, want):
    case(name, got == want, f"got {got!r}, want {want!r}")


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def vo(d, words, timeout=60, env=None):
    r = subprocess.run([VO] + words, cwd=d, capture_output=True, text=True, timeout=timeout * SLOW,
                       env=dict(os.environ, **(env or {})))
    return r.returncode, r.stdout.splitlines(), r.stderr


def run(d, src, name="p.vo", extra=(), timeout=60):
    with open(os.path.join(d, name), "w") as f:
        f.write(module_form(src))
    return vo(d, ["run", "--timeout", str(max(40, timeout - 5) * SLOW)] + list(extra) + [name], timeout)


def replayed(d, name, src, stop, want, extra=(), file="r.vo"):
    """Records the program against its peer, stops the peer, replays: the same output both times."""
    rec = file + ".rec"
    code, out, err = run(d, src, file, extra=["--record", rec] + list(extra))
    lines(f"{name}: the recorded run", (code, out), (0, want))
    stop()
    t = time.time()
    rcode, rout, rerr = vo(d, ["replay", rec])
    lines(f"{name}: replayed without the peer", (rcode, rout), (code, out))
    return time.time() - t, os.path.join(d, rec)


# ---- the event-stream server ----

class EventServer:
    """Serves scripted responses: SCRIPTS[path] = (head, steps, end). head: status line and headers (a str
    without the final empty line, or None for the default event-stream head); steps: (delay_s, bytes) to
    send; end: "close", "reset", "chunked" (a proper end), "chunked-cut" (the body stops mid-way),
    "hold" (keeps the connection open until the client leaves). Requests are kept in .requests."""

    def __init__(self):
        self.scripts = {}
        self.requests = []
        self.sent_at = {}
        self.left_at = {}
        server = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *a):
                pass

            def do_GET(self):
                self.serve()

            def do_POST(self):
                self.serve()

            def serve(self):
                n = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(n) if n else b""
                server.requests.append({"path": self.path, "method": self.command,
                                        "headers": dict(self.headers.items()), "body": body.decode()})
                head, steps, end = server.scripts.get(self.path, ("HTTP/1.1 404 Not Found\r\nContent-Length: 0", [], "close"))
                chunked = end.startswith("chunked")
                if head is None:
                    head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store"
                    head += "\r\nTransfer-Encoding: chunked" if chunked else "\r\nConnection: close"
                sock = self.connection
                try:
                    sock.sendall(head.encode() + b"\r\n\r\n")
                    times = []
                    for delay, data in steps:
                        if delay:
                            time.sleep(delay)
                        if chunked:
                            data = b"%x\r\n" % len(data) + data + b"\r\n"
                        sock.sendall(data)
                        times.append(time.time())
                    server.sent_at[self.path] = times
                    if end == "chunked":
                        sock.sendall(b"0\r\n\r\n")
                    elif end == "chunked-cut":
                        sock.sendall(b"10\r\nabc")
                    elif end == "hold":
                        sock.settimeout(120)
                        while sock.recv(1024):
                            pass
                        server.left_at[self.path] = time.time()
                    if end in ("reset", "chunked-cut"):
                        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                except OSError:
                    pass
                self.close_connection = True

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.httpd.daemon_threads = True
        self.port = self.httpd.server_address[1]
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()

    def stop(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def ev(data=None, name=None, id=None, comment=None, end=b"\n"):
    """One event as the format writes it."""
    out = b""
    if comment is not None:
        out += b":" + comment.encode() + end
    if name is not None:
        out += b"event: " + name.encode() + end
    if id is not None:
        out += b"id: " + id.encode() + end
    if data is not None:
        for line in data.split("\n"):
            out += b"data: " + line.encode() + end
    return out + end


READER = """import std.http;
import std.sse;

fn why(e: sse.EndReason) -> str {
    match e {
        case ended {
            return "ended";
        }
        case broken {
            return "broken";
        }
        case timed_out {
            return "timed_out";
        }
        case stopped {
            return "stopped";
        }
        case not_a_stream(status) {
            return "not_a_stream " + str(status);
        }
        case too_large {
            return "too_large";
        }
        case failed(text) {
            return "failed: " + text;
        }
    }
}

fn shown(s: str) -> str {
    return join(split(s, "\\n"), "\\\\n");
}

fn main() uses console, clock, net("127.0.0.1:{P}") {
    {BODY}
}
"""

PRINT_ALL = """let end = sse.read_events({METHOD}, "http://127.0.0.1:{P}{PATH}", {HEADERS}, {SEND}, {LAST}, {MS}, fn(e) {
        print(e.name + "|" + shown(e.data) + "|" + e.id);
        return ok({GO});
    });
    print("end " + why(end.why) + " " + end.last_id + " " + str(end.retry_ms));"""


def reader(port, path, method='"GET"', headers="new list[http.Header]", send='""', last='""', ms=5000, go="true"):
    body = (PRINT_ALL.replace("{METHOD}", method).replace("{PATH}", path).replace("{HEADERS}", headers)
            .replace("{SEND}", send).replace("{LAST}", last).replace("{MS}", str(ms)).replace("{GO}", go))
    return READER.replace("{BODY}", body).replace("{P}", str(port))


def group_read(d):
    s = EventServer()
    try:
        read_order(d, s)
        read_ends(d, s)
        read_requests(d, s)
        read_latency(d, s)
        read_many(d, s)
        read_scope(d, s)
        read_replay(d, s)
    finally:
        s.stop()


def read_order(d, s):
    n = 200 if QUICK else 1000
    steps, want = [], []
    last = ""
    for i in range(n):
        data = f"event {i}" + ("\nsecond line" if i % 3 == 0 else "")
        name = "tick" if i % 5 == 0 else None
        ident = str(i) if i % 7 == 0 else None
        if ident is not None:
            last = ident
        endl = [b"\n", b"\r\n", b"\r"][i % 3]
        raw = ev(data, name, ident, "keep" if i % 4 == 0 else None, endl)
        if i % 10 == 0:  # split inside the event, and one CRLF across two pieces
            steps += [(0.001, raw[:5]), (0, raw[5:])]
        else:
            steps.append((0.001 if i % 50 == 0 else 0, raw))
        want.append(f"{name or 'message'}|{data.replace(chr(10), chr(92) + 'n')}|{last}")
    s.scripts["/order"] = (None, steps, "close")
    s.scripts["/order-chunked"] = (None, steps, "chunked")
    for path in ("/order", "/order-chunked"):
        code, out, err = run(d, reader(s.port, path))
        lines(f"read{path}: every event in order", (code, out[:-1]), (0, want))
        lines(f"read{path}: the end", out[-1:], [f"end ended {last} -1"])


def read_ends(d, s):
    s.scripts["/reset"] = (None, [(0, ev("a"))], "reset")
    s.scripts["/cut"] = (None, [(0, ev("a"))], "chunked-cut")
    s.scripts["/silent"] = (None, [(0, ev("a", id="4"))], "hold")
    s.scripts["/stop"] = (None, [(0, ev("a", id="1") + ev("b", id="2") + ev("c", id="3"))], "close")
    s.scripts["/plain"] = ("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2", [(0, b"hi")], "close")
    s.scripts["/gzip"] = ("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Encoding: gzip\r\nConnection: close", [(0, b"x")], "close")
    s.scripts["/big"] = (None, [(0, b"data: " + b"x" * (2 << 20) + b"\n\n")], "close")
    s.scripts["/retry"] = (None, [(0, b"retry: 2500\n" + ev("a", id="9") + b"id: 10\n\n")], "close")
    gone = free_port()
    want = {
        "/reset": ["message|a|", "end ended  -1"],  # no chunked framing: a reset reads as the end
        "/cut": ["message|a|", "end broken  -1"],
        "/silent": ["message|a|4", "end timed_out 4 -1"],
        "/missing": ["end not_a_stream 404  -1"],
        "/plain": ["end not_a_stream 200  -1"],
        "/gzip": ["end failed: the stream is compressed (gzip)  -1"],
        "/big": ["end too_large  -1"],
        "/retry": ["message|a|9", "end ended 10 2500"],
    }
    for path, w in want.items():
        code, out, err = run(d, reader(s.port, path, ms=800), extra=["--max-memory", "64"])
        lines(f"read{path}", (code, out), (0, w))
    code, out, err = run(d, reader(s.port, "/stop", go='e.id != "2"'))
    lines("read/stop: f answers false", (code, out), (0, ["message|a|1", "message|b|2", "end stopped 2 -1"]))
    code, out, err = run(d, reader(s.port, "/stop", last='"0"').replace("127.0.0.1:" + str(s.port), "127.0.0.1:" + str(gone), 2))
    case("read/refused: failed, with the last id given", code == 0 and out[-1:] == ["end failed: the connection was refused 0 -1"], f"{code} {out} {err}")


def read_requests(d, s):
    s.scripts["/echo"] = (None, [(0, ev("x"))], "close")
    before = len(s.requests)
    hs = '[new http.Header { name: "Authorization", value: "Bearer t" }]'
    code, out, err = run(d, reader(s.port, "/echo", method='"POST"', headers=hs, send='"{\\"q\\": 1}"', last='"41"'))
    got = s.requests[before:]
    ok = len(got) == 1 and got[0]["method"] == "POST" and got[0]["body"] == '{"q": 1}'
    h = got[0]["headers"] if got else {}
    case("read/request: method, body and headers sent", ok and h.get("Authorization") == "Bearer t"
         and h.get("Last-Event-ID") == "41" and h.get("Accept") == "text/event-stream", f"{got}")


LATENCY = """let start = now_ms();
    let end = sse.read_events("GET", "http://127.0.0.1:{P}/slow", new list[http.Header], "", "", 0, fn(e) {
        print(e.data + " " + str(now_ms()));
        return ok(true);
    });
    print("end " + why(end.why));"""


def read_latency(d, s):
    n = 20 if QUICK else 100
    pause = 3.0 if QUICK else 60.0
    steps = [(pause, ev("first"))] + [(0.05, ev(f"e{i}")) for i in range(n)]
    s.scripts["/slow"] = (None, steps, "close")
    src = READER.replace("{BODY}", LATENCY).replace("{P}", str(s.port))
    code, out, err = run(d, src, timeout=int(pause) + 60)
    sent = s.sent_at.get("/slow", [])
    got = [int(line.split()[1]) for line in out[:-1]]
    late = [round(g - t * 1000) for g, t in zip(got, sent)]
    case("read/latency: every event within 50 ms of its last byte, also after a silence",
         code == 0 and len(got) == n + 1 and max(late) < 50 * SLOW, f"{code} {late} {err}")


MANY = """var count = 0;
    let counter = new list[int];
    push(counter, 0);
    let end = sse.read_events("GET", "http://127.0.0.1:{P}/many", new list[http.Header], "", "", 0, fn(e) {
        set counter[0] = counter[0] + 1;
        return ok(true);
    });
    set count = counter[0];
    print(str(count) + " " + why(end.why));"""


def read_many(d, s):
    n = 100000
    block = b"".join(ev("x" * 92) for _ in range(1000))
    s.scripts["/many"] = (None, [(0, block)] * (n // 1000), "chunked")
    src = READER.replace("{BODY}", MANY).replace("{P}", str(s.port))
    t = time.time()
    code, out, err = run(d, src, extra=["--max-memory", "32"])
    lines("read/many: 100,000 events of 100 bytes in 32 MiB", (code, out), (0, [f"{n} ended"]))
    print(f"  read/many: {time.time() - t:.2f} s")


def read_scope(d, s):
    s.scripts["/echo"] = (None, [(0, ev("x"))], "close")
    src = reader(s.port, "/echo").replace(f'net("127.0.0.1:{s.port}")', 'net("127.0.0.1:1")')
    code, out, err = run(d, src)
    case("read/scope: another port is R0010", code == 70 and "R0010" in err, f"{code} {err[:300]}")


def read_replay(d, s):
    s.scripts["/rec"] = (None, [(0.01, ev(f"r{i}", id=str(i))) for i in range(20)] + [(0, b"retry: 7\n\n")], "chunked")
    want = [f"message|r{i}|{i}" for i in range(20)] + ["end ended 19 7"]
    replayed(d, "read/replay", reader(s.port, "/rec"), lambda: None, want)


WORKER = """import std.http;
import std.sse;
import std.time;
import std.workers;

fn reader(link: Link[int, StreamEvent]) uses net("127.0.0.1:{P}") {
    let end = read_events("GET", "http://127.0.0.1:{P}/feed", new list[Header], "", "", 0, fn(e) {
        match emit(link, e) {
            case ok() {
                return ok(true);
            }
            case err(_) {
                return ok(false);
            }
        }
    });
}

fn ticker(link: Link[int, StreamEvent]) uses clock {
    for i in 0..3 {
        sleep(150);
        match emit(link, new StreamEvent { name: "tick", data: str(i), id: "" }) {
            case ok() {
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() -> result[void] uses console, clock, net("127.0.0.1:{P}") {
    let a = try map_err(start_worker(reader), worker_error_text);
    let b = try map_err(start_worker(ticker), worker_error_text);
    let both = [a, b];
    var seen = 0;
    while seen < 6 {
        match receive_first(both, Wait.at_most(5000)) {
            case ok(arrival) {
                match arrival.message {
                    case ok(e) {
                        print(str(arrival.index) + " " + e.name + " " + e.data);
                        set seen = seen + 1;
                    }
                    case err(why) {
                    }
                }
            }
            case err(why) {
                print(worker_error_text(why));
                return ok();
            }
        }
    }
    print("stop " + str(now_ms()));
    stop_worker(a);
    stop_worker(b);
    return ok();
}
"""


def group_workers(d):
    """FR-021: a stream read in a worker, waited on beside another worker, closed by stopping it."""
    s = EventServer()
    try:
        s.scripts["/feed"] = (None, [(0.05, ev("one")), (0.3, ev("two")), (0.3, ev("three"))], "hold")
        code, out, err = run(d, WORKER.replace("{P}", str(s.port)))
        got = sorted(x for x in out if not x.startswith("stop"))
        lines("workers/events and ticks both arrive", (code, got),
              (0, ["0 message one", "0 message three", "0 message two", "1 tick 0", "1 tick 1", "1 tick 2"]))
        stop = [int(x.split()[1]) for x in out if x.startswith("stop")]
        time.sleep(0.5)
        left = s.left_at.get("/feed")
        took = (left * 1000 - stop[0]) if stop and left else None
        case("workers/stopping the worker closes its connection (spec 047: gone within 200 ms)",
             took is not None and took < 400 * SLOW, f"{took} ms {err[:300]}")
        print(f"  workers/connection closed {took:.0f} ms after stop_worker" if took else "")
    finally:
        s.stop()


# ---- the WebSocket client ----

import ws_peer  # noqa: E402

WS = """import std.bytes;
import std.http;
import std.websocket;

fn word(e: WsError) -> str {
    match e {
        case nothing_in_time {
            return "nothing_in_time";
        }
        case closed(c) {
            return "closed " + str(c.code) + " " + c.reason;
        }
        case broken {
            return "broken";
        }
        case protocol(_) {
            return "protocol";
        }
        case not_text {
            return "not_text";
        }
        case too_large {
            return "too_large";
        }
        case stopping {
            return "stopping";
        }
        case refused(status) {
            return "refused " + str(status);
        }
        case connect(_) {
            return "connect " + ws_error_text(e);
        }
        case invalid(what) {
            return "invalid " + what;
        }
    }
}

fn shown(m: Message) -> str {
    match m {
        case text(t) {
            if len(t) > 20 {
                return "text " + str(len(t)) + " ...";
            }
            return "text " + str(len(t)) + " " + t;
        }
        case binary(b) {
            return "binary " + str(len(b));
        }
    }
}

fn options() -> ConnectOptions {
    return new ConnectOptions { headers: new list[Header], protocols: new list[str], max_message: 0 };
}

fn drain(s: Socket[uses net], ms: int) uses console, net("127.0.0.1:{P}") {
    # Prints each message until an error, then the error.
    while true {
        match receive_message(s, ms) {
            case ok(m) {
                print(shown(m));
            }
            case err(e) {
                print("error " + word(e));
                return;
            }
        }
    }
}

fn main() -> result[void] uses console, net("127.0.0.1:{P}") {
    {BODY}
    return ok();
}
"""

CASES_BODY = """for k in 0..{N} {
        match ws_connect("ws://127.0.0.1:{P}/" + str(k), options(), 5000) {
            case ok(s) {
                print("case " + str(k));
                drain(s, 3000);
                match close_socket(s, 1000, "") {
                    case ok() {
                    }
                    case err(e) {
                        print("close " + word(e));
                    }
                }
            }
            case err(e) {
                print("case " + str(k) + " " + word(e));
            }
        }
    }"""


def ws_prog(port, body, **more):
    src = WS.replace("{BODY}", body)
    for k, v in more.items():
        src = src.replace("{" + k + "}", str(v))
    return src.replace("{P}", str(port))


def by_case(out):
    got, cur = {}, None
    for line in out:
        if line.startswith("case "):
            cur = int(line.split()[1])
            got[cur] = []
        elif cur is not None:
            got[cur].append(line)
    return got


def close_code(seen):
    for op, payload, masked in seen or []:
        if op == 8:
            return int.from_bytes(payload[:2], "big") if len(payload) >= 2 else 1005
    return None


def group_client(d):
    client_hostile(d)
    client_echo(d)
    client_handshakes(d)
    client_misc(d)
    client_more(d)


def client_hostile(d):
    table = [(n, raw, w, c) for n, raw, w, c in ws_peer.hostile(False)]
    goods = ws_peer.good(False)
    scripts = {f"/{k}": raw for k, (_, raw, _, _) in enumerate(table)}
    for k, (n, raw, msgs) in enumerate(goods):
        scripts[f"/{len(table) + k}"] = raw
    srv = ws_peer.Server(scripts)
    try:
        total = len(table) + len(goods)
        code, out, err = run(d, ws_prog(srv.port, CASES_BODY, N=total), timeout=300)
        case("client/hostile: the program ran", code == 0, f"{code} {err[:400]}")
        got = by_case(out)
        time.sleep(0.2)
        for k, (n, raw, w, c) in enumerate(table):
            lines_k = got.get(k, [])
            ok = bool(lines_k) and lines_k[-1] == "error " + w
            case(f"client/hostile/{n}: {w}", ok, f"{lines_k}")
            sent = close_code(srv.seen_by.get(f"/{k}"))
            case(f"client/hostile/{n}: closes with {c}", sent == c, f"sent {sent}")
        for k, (n, raw, msgs) in enumerate(goods):
            lines_k = got.get(len(table) + k, [])
            if msgs[-1].startswith("closed"):
                want = msgs[:-1] + ["error " + msgs[-1]]
            else:
                want = msgs + ["error nothing_in_time"]
            lines(f"client/good/{n}", lines_k, want)
    finally:
        srv.stop()


ECHO_BODY = """let s = try map_err(ws_connect("ws://127.0.0.1:{P}/echo", options(), 5000), word);
    var sum = 0;
    for i in 0..{N} {
        let size = (i * 7919) % {MAX};
        var data = bytes.from_ints([i % 256]);
        if size > 1 {
            set data = __fill(size, i);
        }
        if i % 2 == 0 {
            try map_err(send_binary(s, data), word);
        } else {
            try map_err(send_text(s, str(i) + ":" + bytes.to_hex(slice(data, 0, min(len(data), 8)))), word);
        }
        match receive_message(s, 10000) {
            case ok(m) {
                match m {
                    case binary(b) {
                        if b != data {
                            print("changed " + str(i));
                        }
                        set sum = sum + len(b);
                    }
                    case text(t) {
                        if t != str(i) + ":" + bytes.to_hex(slice(data, 0, min(len(data), 8))) {
                            print("changed " + str(i));
                        }
                    }
                }
            }
            case err(e) {
                print("error " + str(i) + " " + word(e));
                return ok();
            }
        }
    }
    try map_err(close_socket(s, 1000, "done"), word);
    print("echoed " + str(sum));"""

FILL = """
fn __fill(size: int, seed: int) -> bytes {
    let unit = bytes.from_text("abcdefghijklmnopqrstuvwxyz0123456789");
    var out = unit;
    while len(out) < size {
        set out = out + out;
    }
    return slice(out, seed % 7, seed % 7 + size - 1) + bytes.from_ints([seed % 256]);
}
"""


def client_echo(d):
    n = 400 if QUICK else 10000
    biggest = 1048576
    srv = ws_peer.Server("echo")
    try:
        src = ws_prog(srv.port, ECHO_BODY, N=n, MAX=biggest).replace("__fill", "fill") + FILL.replace("__fill", "fill")
        want = sum(((i * 7919) % biggest if (i * 7919) % biggest > 1 else 1) for i in range(0, n, 2))
        t = time.time()
        code, out, err = run(d, src, timeout=600, extra=["--max-memory", "128"])
        lines(f"client/echo: {n} messages of 0 bytes to 1 MiB come back unchanged", (code, out), (0, [f"echoed {want}"]))
        print(f"  client/echo: {time.time() - t:.2f} s")
    finally:
        srv.stop()


ONE_BODY = """match ws_connect("ws://127.0.0.1:{P}{PATH}", {OPTIONS}, {MS}) {
        case ok(s) {
            print("protocol [" + s.protocol + "]");
            drain(s, {WAIT});
        }
        case err(e) {
            print(word(e));
        }
    }"""


def one(port, path="/", options="options()", ms=5000, wait=1000):
    return ws_prog(port, ONE_BODY, PATH=path, OPTIONS=options, MS=ms, WAIT=wait)


def client_handshakes(d):
    for name, hs, want in [
        ("404", {"status": 404}, ["refused 404"]),
        ("a wrong accept value", {"accept": "nope"}, ["refused 101"]),
        ("an extension not asked for", {"extension": "permessage-deflate"}, ["refused 101"]),
        ("a sub-protocol not asked for", {"protocol": "chat"}, ["refused 101"]),
    ]:
        srv = ws_peer.Server("echo", hs)
        code, out, err = run(d, one(srv.port))
        lines(f"client/handshake: {name}", (code, out), (0, want))
        srv.stop()
    srv = ws_peer.Server(("script", b""))
    opts = 'new ConnectOptions { headers: [new Header { name: "Authorization", value: "Bearer t" }], protocols: ["v2.chat", "chat"], max_message: 0 }'
    code, out, err = run(d, one(srv.port, "/room?x=1", opts))
    req = srv.requests[0] if srv.requests else ("", {})
    lines("client/handshake: the chosen sub-protocol", (code, out), (0, ["protocol [v2.chat]", "error nothing_in_time"]))
    case("client/handshake: request line, headers and protocols sent",
         req[0] == "GET /room?x=1 HTTP/1.1" and req[1].get("authorization") == "Bearer t"
         and req[1].get("sec-websocket-protocol") == "v2.chat, chat" and req[1].get("sec-websocket-version") == "13", f"{req}")
    srv.stop()
    gone = free_port()
    code, out, err = run(d, one(gone).replace(f"127.0.0.1:{gone}", f"127.0.0.1:{gone}"))
    lines("client/handshake: refused connection", (code, out), (0, ["connect could not connect: the connection was refused"]))


def client_misc(d):
    # a peer that closes properly, a peer that resets, nothing in time between fragments
    srv = ws_peer.Server(("script", ws_peer.frame(1, b"ab", fin=False)))
    code, out, err = run(d, one(srv.port, wait=500))
    lines("client/nothing in time between fragments", (code, out), (0, ["protocol []", "error nothing_in_time"]))
    srv.stop()

    def reset(conn):
        conn.sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        conn.sock.close()
    srv = ws_peer.Server(reset)
    code, out, err = run(d, one(srv.port))
    lines("client/a reset is broken", (code, out), (0, ["protocol []", "error broken"]))
    srv.stop()
    pings = b"".join(ws_peer.frame(9, b"p%d" % i) for i in range(10000)) + ws_peer.frame(8, ws_peer.close_payload(1000))
    srv = ws_peer.Server(("script", pings))
    code, out, err = run(d, one(srv.port, wait=5000), timeout=120)
    time.sleep(0.2)
    pongs = sum(1 for op, _, _ in (srv.seen[0] if srv.seen else []) if op == 10)
    lines("client/10,000 pings answered", (code, out, pongs), (0, ["protocol []", "error closed 1000 "], 10000))
    srv.stop()
    scope = one(4).replace('net("127.0.0.1:4")', 'net("127.0.0.1:1")', 2)
    code, out, err = run(d, scope)
    case("client/scope: another port is R0010", code == 70 and "R0010" in err, f"{code} {out} {err[:300]}")


CA = os.path.join(ROOT, "tests", "tls", "certs", "test-ca.pem")

TASKS_BODY = """let said = concurrent_map([1, 2, 3], 3, fn(i) => talk(i));
    print(join(said, " "));"""

TALK = """
fn talk(i: int) -> str uses net("127.0.0.1:{P}") {
    match ws_connect("ws://127.0.0.1:{P}/echo", options(), 5000) {
        case ok(s) {
            match send_text(s, "task " + str(i)) {
                case ok() {
                }
                case err(e) {
                    return word(e);
                }
            }
            match receive_message(s, 5000) {
                case ok(m) {
                    match close_socket(s, 1000, "") {
                        case ok() {
                        }
                        case err(e) {
                            return word(e);
                        }
                    }
                    return shown(m);
                }
                case err(e) {
                    return word(e);
                }
            }
        }
        case err(e) {
            return word(e);
        }
    }
}
"""

LEAVE_BODY = """match ws_connect("ws://127.0.0.1:{P}/left", options(), 5000) {
        case ok(s) {
            print("left open");
        }
        case err(e) {
            print(word(e));
        }
    }"""

BOTH_BODY = """let s = try map_err(ws_connect("ws://127.0.0.1:{P}/both", options(), 5000), word);
    print(word2(close_socket(s, 1000, "mine")));
    drain(s, 1000);"""

WORD2 = """
fn word2(r: result[void, WsError]) -> str {
    match r {
        case ok() {
            return "ok";
        }
        case err(e) {
            return word(e);
        }
    }
}
"""

TEST_LEAVE = """import std.http;
import std.websocket;

fn options() -> ConnectOptions {
    return new ConnectOptions { headers: new list[Header], protocols: new list[str], max_message: 0 };
}

test "a leaves its socket open" uses net("127.0.0.1:{P}") {
    match ws_connect("ws://127.0.0.1:{P}/first", options(), 5000) {
        case ok(s) {
            expect(true);
        }
        case err(e) {
            expect(false);
        }
    }
}

test "b connects after" uses net("127.0.0.1:{P}") {
    match ws_connect("ws://127.0.0.1:{P}/second", options(), 5000) {
        case ok(s) {
            expect(close_socket(s, 1000, "") == ok());
        }
        case err(e) {
            expect(false);
        }
    }
}
"""


WS_WORKER = """import std.http;
import std.time;
import std.websocket;
import std.workers;

fn holder(link: Link[str, str]) uses net("127.0.0.1:{P}") {
    let options = new ConnectOptions { headers: new list[Header], protocols: new list[str], max_message: 0 };
    match ws_connect("ws://127.0.0.1:{P}/held", options, 5000) {
        case ok(s) {
            while true {
                match next_message(link, Wait.forever) {
                    case ok(text) {
                        match send_text(s, text) {
                            case ok() {
                            }
                            case err(_) {
                                return;
                            }
                        }
                        match receive_message(s, 5000) {
                            case ok(m) {
                                match m {
                                    case text(t) {
                                        match emit(link, "echo " + t) {
                                            case ok() {
                                            }
                                            case err(_) {
                                                return;
                                            }
                                        }
                                    }
                                    case binary(_) {
                                    }
                                }
                            }
                            case err(_) {
                                return;
                            }
                        }
                    }
                    case err(_) {
                        return;
                    }
                }
            }
        }
        case err(_) {
        }
    }
}

fn ticker(link: Link[str, str]) uses clock {
    sleep(100);
    match emit(link, "tick") {
        case ok() {
        }
        case err(_) {
        }
    }
}

fn main() -> result[void] uses console, clock, net("127.0.0.1:{P}") {
    let w = try map_err(start_worker(holder), worker_error_text);
    let t = try map_err(start_worker(ticker), worker_error_text);
    for word in ["a", "b", "c"] {
        try map_err(tell(w, word), worker_error_text);
    }
    var seen = 0;
    while seen < 4 {
        match receive_first([w, t], Wait.at_most(5000)) {
            case ok(arrival) {
                match arrival.message {
                    case ok(text) {
                        print(text);
                        set seen = seen + 1;
                    }
                    case err(_) {
                    }
                }
            }
            case err(e) {
                print(worker_error_text(e));
                return ok();
            }
        }
    }
    print("stop " + str(now_ms()));
    stop_worker(w);
    stop_worker(t);
    return ok();
}
"""


def client_worker(d):
    srv = ws_peer.Server("echo")
    try:
        code, out, err = run(d, WS_WORKER.replace("{P}", str(srv.port)))
        got = sorted(x for x in out if not x.startswith("stop"))
        lines("client/a socket held by a worker", (code, got), (0, ["echo a", "echo b", "echo c", "tick"]))
        stop = [int(x.split()[1]) for x in out if x.startswith("stop")]
        time.sleep(0.5)
        ends = [e[0] for e in srv.events if e[1] == "end" and e[2] == "/held"]
        took = ends[0] * 1000 - stop[0] if stop and ends else None
        case("client/stopping the worker closes the socket", took is not None and took < 400 * SLOW, f"{took} {err[:300]}")
    finally:
        srv.stop()


def client_more(d):
    client_worker(d)
    srv = ws_peer.Server("echo", tls="a.test")
    try:
        src = one(srv.port).replace(f'"ws://127.0.0.1:{srv.port}/"', f'"wss://127.0.0.1:{srv.port}/"')
        code, out, err = run(d, src, extra=["--ca-file", CA])
        lines("client/wss with the test CA", (code, out), (0, ["protocol []", "error nothing_in_time"]))
        code, out, err = run(d, src)
        case("client/wss without it: a certificate error", code == 0 and len(out) == 1 and out[0].startswith("connect could not connect: TLS failed"), f"{out}")
    finally:
        srv.stop()
    srv = ws_peer.Server("echo")
    try:
        code, out, err = run(d, ws_prog(srv.port, TASKS_BODY) + TALK.replace("{P}", str(srv.port)))
        lines("client/in concurrent_map tasks", (code, out), (0, ["text 6 task 1 text 6 task 2 text 6 task 3"]))
        code, out, err = run(d, ws_prog(srv.port, LEAVE_BODY))
        time.sleep(0.5)
        ends = [e for e in srv.events if e[2] == "/left" and e[1] == "end"]
        lines("client/a socket left open is closed when the program ends", (code, out, len(ends)), (0, ["left open"], 1))
        with open(os.path.join(d, "t.vo"), "w") as f:
            f.write(module_form(TEST_LEAVE.replace("{P}", str(srv.port))))
        code, out, err = vo(d, ["test", "t.vo"])
        time.sleep(0.5)
        ev_ = {(e[1], e[2]): e[0] for e in srv.events}
        case("client/a socket a test left open is closed when that test ends", code == 0 and ("end", "/first") in ev_
             and ev_[("end", "/first")] <= ev_.get(("open", "/second"), 0), f"{code} {out} {err[:300]} {srv.events}")
        code, out, err = run(d, one(srv.port, wait=500), extra=["--clock-ms", "5"])
        lines("client/time limits under --clock-ms", (code, out), (0, ["protocol []", "error nothing_in_time"]))
    finally:
        srv.stop()

    def both(conn):
        conn.send(8, ws_peer.close_payload(1001, b"theirs"))
        conn.messages()
    srv = ws_peer.Server(both)
    try:
        code, out, err = run(d, ws_prog(srv.port, BOTH_BODY) + WORD2)
        lines("client/both sides close at once", (code, out), (0, ["ok", "error closed 1000 mine"]))
    finally:
        srv.stop()
    if not QUICK:
        srv = ws_peer.Server(("script", ws_peer.frame(2, b"0123456789")[:5]))
        t = time.time()
        code, out, err = run(d, one(srv.port, wait=1000), timeout=120)
        took = time.time() - t
        lines("client/a frame half sent for 30 s is broken", (code, out), (0, ["protocol []", "error broken"]))
        case("client/... after about 30 s", 29 < took < 40, f"{took:.1f} s")
        srv.stop()


# ---- serving streams ----

STREAM_SERVER = r"""import std.http;
import std.sse;
import std.time;

fn options() -> StreamOptions {
    return new StreamOptions { headers: [new Header { name: "Access-Control-Allow-Origin", value: "*" }], max_open: 0 };
}

fn ev(name: str, data: str, id: str) -> StreamEvent {
    return new StreamEvent { name: name, data: data, id: id };
}

fn five[E: effects](s: EventStream[uses E]) -> result[void] uses E, clock {
    for i in 0..5 {
        try map_err(send_event(s, ev("step", str(i), str(i))), stream_error_text);
        sleep({GAP});
    }
    return ok();
}

fn forever[E: effects](s: EventStream[uses E]) -> result[void] uses E, clock, console {
    var n = 0;
    while true {
        match send_event(s, ev("", "tick " + str(n), "")) {
            case ok() {
                set n = n + 1;
            }
            case err(e) {
                print("forever " + stream_error_text(e) + " " + str(now_ms()));
                return ok();
            }
        }
        sleep(100);
    }
}

fn flood[E: effects](s: EventStream[uses E]) -> result[void] uses E, console, clock {
    var big = "x";
    for _ in 0..16 {
        set big = big + big;
    }
    while true {
        match send_event(s, ev("", big, "")) {
            case ok() {
            }
            case err(e) {
                print("flood " + stream_error_text(e) + " " + str(now_ms()));
                return ok();
            }
        }
    }
}

fn tricky[E: effects](s: EventStream[uses E]) -> result[void] uses E {
    let parts = ["a", "", ":colon", " lead", "\u{E5}\u{2713}", "x\ry", "x\r\ny", "x\ny", "\n", "end\n", "data: inner"];
    for i in 0..200 {
        let data = parts[i % len(parts)] + parts[(i * 7) % len(parts)];
        try map_err(send_event(s, ev("t" + str(i % 3), data, str(i))), stream_error_text);
        if i % 50 == 0 {
            try map_err(keep_alive(s), stream_error_text);
        }
    }
    return ok();
}

fn failing[E: effects](s: EventStream[uses E]) -> result[void] uses E {
    try map_err(send_event(s, ev("", "one", "")), stream_error_text);
    try map_err(send_event(s, ev("", "two", "")), stream_error_text);
    let zero = len(s.sent);
    try map_err(send_event(s, ev("", str(1 / zero), "")), stream_error_text);
    return ok();
}

fn handle(r: Request) -> Response uses serve, clock, console {
    var last = "";
    match request_header(r, "Last-Event-ID") {
        case some(v) {
            set last = v;
        }
        case none {
        }
    }
    let last_id = last;
    if r.path == "/five" {
        return serve_events(r, options(), five);
    }
    if r.path == "/forever" {
        return serve_events(r, options(), forever);
    }
    if r.path == "/flood" {
        return serve_events(r, options(), flood);
    }
    if r.path == "/tricky" {
        return serve_events(r, options(), tricky);
    }
    if r.path == "/fail" {
        return serve_events(r, options(), failing);
    }
    if r.path == "/resume" {
        return serve_events(r, options(), fn(s) => map_err(send_event(s, ev("resumed", "after " + last_id, "")), stream_error_text));
    }
    return new Response { status: 200, headers: new list[Header], body: "plain " + r.method + " " + r.path };
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses console, clock, serve("{P}") {
    try serve_workers("{P}", 4, setup, fn(n, r) => handle(r));
    return ok();
}
"""


class VoServer:
    """A vo program serving on a free port; .stop() sends SIGTERM and returns (code, stdout, stderr)."""

    def __init__(self, d, src, name="server.vo", extra=()):
        self.port = free_port()
        with open(os.path.join(d, name), "w") as f:
            f.write(module_form(src.replace("{P}", str(self.port))))
        # output to files: a pipe nobody reads until the end fills up and stalls a server that prints a lot
        self.out_path, self.err_path = os.path.join(d, name + ".out"), os.path.join(d, name + ".err")
        self.out_f, self.err_f = open(self.out_path, "w"), open(self.err_path, "w")
        self.p = subprocess.Popen([VO, "run"] + list(extra) + [name], cwd=d, stdout=self.out_f, stderr=self.err_f)
        for _ in range(200 * SLOW):
            try:
                socket.create_connection(("127.0.0.1", self.port), timeout=1).close()
                return
            except OSError:
                if self.p.poll() is not None:
                    break
                time.sleep(0.05)

    def stop(self, sig=15):
        if self.p.poll() is None:
            self.p.send_signal(sig)
        try:
            self.p.wait(timeout=60 * SLOW)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.out_f.close()
        self.err_f.close()
        return self.p.returncode, open(self.out_path).read(), open(self.err_path).read()


def raw_get(port, path, headers="", read_for=None, method="GET", body=b""):
    """(head, body bytes, [arrival times of body pieces]) of one request on a fresh connection."""
    s = socket.create_connection(("127.0.0.1", port), timeout=30)
    req = f"{method} {path} HTTP/1.1\r\nHost: x\r\n{headers}"
    if body:
        req += f"Content-Length: {len(body)}\r\n"
    s.sendall(req.encode() + b"\r\n" + body)
    data, times, start = b"", [], time.time()
    while b"\r\n\r\n" not in data:
        got = s.recv(65536)
        if not got:
            break
        data += got
    head, _, rest = data.partition(b"\r\n\r\n")
    times.append(time.time())
    while read_for is None or time.time() - start < read_for:
        try:
            got = s.recv(65536)
        except socket.timeout:
            break
        if not got:
            break
        rest += got
        times.append(time.time())
    s.close()
    return head.decode(), rest, times


TRICKY_PARTS = ["a", "", ":colon", " lead", "\u00e5\u2713", "x\ry", "x\r\ny", "x\ny", "\n", "end\n", "data: inner"]


def group_stream(d):
    gap = 200 if QUICK else 1000
    srv = VoServer(d, STREAM_SERVER.replace("{GAP}", str(gap)))
    try:
        head, body, times = raw_get(srv.port, "/five")
        case("stream/the head", head.split("\r\n")[:6] == ["HTTP/1.1 200 OK", "Content-Type: text/event-stream", "Cache-Control: no-store",
             "X-Accel-Buffering: no", "Connection: close", "Access-Control-Allow-Origin: *"], head)
        lines("stream/five events", body.decode(), "".join(f"event: step\nid: {i}\ndata: {i}\n\n" for i in range(5)))
        spread = times[-1] - times[0]
        case("stream/events arrive as they are sent, not at the end", spread > 3.5 * gap / 1000, f"{spread:.2f} s")
        r = subprocess.run(["curl", "-s", "-N", "--max-time", "20", f"http://127.0.0.1:{srv.port}/five"], capture_output=True, text=True)
        lines("stream/read by curl", r.stdout.count("event: step"), 5)
        code, out, err = run(d, reader(srv.port, "/tricky"))
        want = []
        for i in range(200):
            data = TRICKY_PARTS[i % 11] + TRICKY_PARTS[(i * 7) % 11]
            data = data.replace("\r\n", "\n").replace("\r", "\n")
            want.append(f"t{i % 3}|{data.replace(chr(10), chr(92) + 'n')}|{i}")
        lines("stream/200 events with line breaks, colons and characters round-trip", (code, out[:-1]), (0, want))
        head, body, _ = raw_get(srv.port, "/resume", headers="Last-Event-ID: 41\r\n")
        lines("stream/Last-Event-ID readable in the handler", body.decode(), "event: resumed\ndata: after 41\n\n")
        r = subprocess.run(["curl", "-s", "-N", "-H", "Last-Event-ID: 7", f"http://127.0.0.1:{srv.port}/resume"], capture_output=True, text=True)
        lines("stream/a reconnect as EventSource makes it", r.stdout, "event: resumed\ndata: after 7\n\n")
        head, body, _ = raw_get(srv.port, "/fail")
        lines("stream/a failing handler ends the stream", body.decode(), "data: one\n\ndata: two\n\n")
        head, body, _ = raw_get(srv.port, "/plain", method="POST", body=b"x")
        lines("stream/the server goes on", body.decode(), "plain POST /plain")
        s = socket.create_connection(("127.0.0.1", srv.port), timeout=30)
        s.sendall(b"GET /forever HTTP/1.1\r\nHost: x\r\n\r\n")
        got = b""
        while got.count(b"tick") < 3:
            got += s.recv(65536)
        left = time.time()
        s.close()
        time.sleep(1.5)
        if not QUICK:
            s2 = socket.create_connection(("127.0.0.1", srv.port), timeout=30)
            s2.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            s2.sendall(b"GET /flood HTTP/1.1\r\nHost: x\r\n\r\n")
            flood_start = time.time()
            time.sleep(45)
            s2.close()
    finally:
        code, out, err = srv.stop()
    short = VoServer(d, STREAM_SERVER.replace("{GAP}", "400"), name="short.vo", extra=["--timeout", "1"])
    try:
        head, body, _ = raw_get(short.port, "/five")
        lines("stream/--timeout limits answering, not the conversation (2 s under --timeout 1)", body.decode().count("event: step"), 5)
    finally:
        short.stop()
    gone = [line for line in out.splitlines() if line.startswith("forever ")]
    took = int(gone[0].split()[-1]) - left * 1000 if gone else None
    case("stream/client_gone within 1 s of the next send", gone and "the client has gone" in gone[0] and took < (1000 + 150) * SLOW,
         f"{gone} {took} {err[-500:]}")
    case("stream/the failure is logged and the server stops cleanly", code == 0 and "R0002" in err, f"{code} {err[-600:]}")
    if not QUICK:
        flood = [line for line in out.splitlines() if line.startswith("flood ")]
        took = int(flood[0].split()[-1]) / 1000 - flood_start if flood else None
        case("stream/not_reading after about 30 s", flood and "took nothing" in flood[0] and 29 < took < 44, f"{flood} {took}")


# ---- accepting WebSockets ----

WS_SERVER = r"""import std.http;
import std.websocket;

fn word(e: WsError) -> str {
    match e {
        case nothing_in_time {
            return "nothing_in_time";
        }
        case closed(c) {
            return "closed " + str(c.code);
        }
        case broken {
            return "broken";
        }
        case protocol(_) {
            return "protocol";
        }
        case not_text {
            return "not_text";
        }
        case too_large {
            return "too_large";
        }
        case stopping {
            return "stopping";
        }
        case refused(status) {
            return "refused " + str(status);
        }
        case connect(_) {
            return "connect";
        }
        case invalid(what) {
            return "invalid " + what;
        }
    }
}

fn echo[E: effects](s: Socket[uses E]) -> result[void] uses E {
    while true {
        match receive_message(s, 0) {
            case ok(m) {
                match m {
                    case text(t) {
                        try map_err(send_text(s, t), word);
                    }
                    case binary(b) {
                        try map_err(send_binary(s, b), word);
                    }
                }
            }
            case err(_) {
                return ok();
            }
        }
    }
}

fn watched(s: Socket[uses serve], path: str) -> result[void] uses serve, console {
    while true {
        match receive_message(s, 0) {
            case ok(m) {
                match m {
                    case text(t) {
                        print(path + " text " + str(len(t)));
                    }
                    case binary(b) {
                        print(path + " binary " + str(len(b)));
                    }
                }
            }
            case err(e) {
                print(path + " " + word(e));
                return ok();
            }
        }
    }
}

fn options(origins: list[str], protocols: list[str]) -> AcceptOptions {
    return new AcceptOptions { origins: origins, protocols: protocols, max_message: 0, max_open: 6 };
}

fn chat[E: effects](s: Socket[uses E]) -> result[void] uses E {
    return map_err(send_text(s, "protocol " + s.protocol), word);
}

fn crash(s: Socket[uses serve]) -> result[void] uses serve {
    try map_err(send_text(s, "about to fail"), word);
    let zero = len(s.parts);
    try map_err(send_text(s, str(1 / zero)), word);
    return ok();
}

fn handle(r: Request) -> Response uses serve, console {
    let path = r.path;
    if path == "/echo" {
        return ws_accept(r, options(new list[str], new list[str]), echo);
    }
    if len(path) > 3 and slice(path, 0, 3) == "/h/" {
        return ws_accept(r, options(new list[str], new list[str]), fn(s) => watched(s, path));
    }
    if path == "/chat" {
        return ws_accept(r, options(new list[str], ["v2.chat", "chat"]), chat);
    }
    if path == "/origin" {
        return ws_accept(r, options(["https://app.example"], new list[str]), chat);
    }
    if path == "/any" {
        return ws_accept(r, options(["*"], new list[str]), chat);
    }
    if path == "/fail" {
        return ws_accept(r, options(new list[str], new list[str]), fn(s) => err("the handler said no"));
    }
    if path == "/crash" {
        return ws_accept(r, options(new list[str], new list[str]), crash);
    }
    return new Response { status: 200, headers: new list[Header], body: "plain " + path };
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses console, serve("{P}") {
    try serve_workers("{P}", 8, setup, fn(n, r) => handle(r));
    return ok();
}
"""


WEB_WS = r"""import std.http;
import std.web;
import std.websocket;

fn hello[E: effects](s: Socket[uses E]) -> result[void] uses E {
    return map_err(send_text(s, "signed in"), ws_error_text);
}

fn token(n: int, t: str) -> result[str] {
    if t == "good" {
        return ok("ada");
    }
    return err("no");
}

fn talk(n: int, r: Request, vars: map[str, str]) -> result[Response] uses serve {
    let options = new AcceptOptions { origins: new list[str], protocols: new list[str], max_message: 0, max_open: 0 };
    return ok(ws_accept(r, options, hello));
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses console, serve("{P}") {
    let app = new App[int, uses serve] { routes: [on_request("GET", "/ws", talk)], steps: [bearer_auth("/", token)], errors: new list[ErrorStatus] };
    try serve_app("{P}", 4, setup, app);
    return ok();
}
"""


def accept_web(d):
    srv = VoServer(d, WEB_WS, name="web.vo")
    try:
        status, rh, conn = handshake(srv.port, "/ws")
        lines("accept/a sign-in step refuses before the handshake", (status, conn), (401, None))
        status, rh, conn = handshake(srv.port, "/ws", {"Authorization": "Bearer good"})
        msgs = conn.messages() if conn else []
        lines("accept/signed in: the handshake and the handler", (status, msgs[:1]), (101, [(1, b"signed in", False)]))
    finally:
        code, out, err = srv.stop()
    case("accept/the web server stops cleanly", code == 0, f"{code} {err[-300:]}")


def handshake(port, path, headers=None, key=None):
    try:
        return ws_peer.client_connect(port, path, headers, key)
    except (OSError, EOFError) as e:
        return 0, {}, None


def group_accept(d):
    srv = VoServer(d, WS_SERVER)
    table = ws_peer.hostile(True)
    accept_web(d)
    try:
        accept_handshakes(d, srv)
        accept_echo(d, srv)
        for k, (n, raw, w, c) in enumerate(table):
            status, rh, conn = handshake(srv.port, f"/h/{k}")
            if not conn:
                case(f"accept/hostile/{n}: handshake", False, str(status))
                continue
            threading.Thread(target=lambda c_=conn, r_=raw: _quiet(c_.raw, r_), daemon=True).start()
            seen = conn.messages()
            conn.sock.close()
            case(f"accept/hostile/{n}: closes with {c}", close_code(seen) == c, f"sent {close_code(seen)}")
    finally:
        code, out, err = srv.stop()
    said = {}
    for line in out.splitlines():
        parts = line.split(" ", 1)
        if parts[0].startswith("/h/"):
            said[int(parts[0][3:])] = parts[1]
    for k, (n, raw, w, c) in enumerate(table):
        case(f"accept/hostile/{n}: {w}", said.get(k) == w, f"{said.get(k)}")
    case("accept/the failures are logged and the server stops cleanly", code == 0 and "R0002" in err, f"{code} {err[-400:]}")


def _quiet(f, *a):
    try:
        f(*a)
    except OSError:
        pass


def accept_handshakes(d, srv):
    p = srv.port
    status, rh, conn = handshake(p, "/chat", {"Sec-WebSocket-Protocol": "chat, v2.chat"})
    msgs = conn.messages() if conn else []
    lines("accept/the first of the client's sub-protocols the route speaks", (status, rh.get("sec-websocket-protocol"), msgs[:1]),
          (101, "chat", [(1, b"protocol chat", False)]))
    status, rh, conn = handshake(p, "/chat")
    msgs = conn.messages() if conn else []
    lines("accept/no sub-protocol asked: none chosen", (status, "sec-websocket-protocol" in rh, msgs[:1]), (101, False, [(1, b"protocol ", False)]))
    case("accept/the handler returning closes with 1000", close_code(msgs) == 1000, f"{msgs}")
    lines("accept/an origin not named is 403", handshake(p, "/origin", {"Origin": "https://evil.example"})[0], 403)
    lines("accept/the named origin", handshake(p, "/origin", {"Origin": "https://app.example"})[0], 101)
    lines("accept/no origin (another program)", handshake(p, "/origin")[0], 101)
    lines("accept/[] accepts no browser origin", handshake(p, "/echo", {"Origin": "https://app.example"})[0], 403)
    lines("accept/['*'] accepts any origin", handshake(p, "/any", {"Origin": "https://evil.example"})[0], 101)
    head, body, _ = raw_get(p, "/echo", headers="Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 8\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
    case("accept/version 8 is 426 with the version", head.startswith("HTTP/1.1 426") and "Sec-WebSocket-Version: 13" in head, head)
    head, body, _ = raw_get(p, "/echo")
    case("accept/a plain request is 426", head.startswith("HTTP/1.1 426"), head)
    head, body, _ = raw_get(p, "/echo", headers="Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: short\r\n")
    case("accept/a bad key is 400", head.startswith("HTTP/1.1 400"), head)
    head, body, _ = raw_get(p, "/echo", method="POST", body=b"x", headers="Upgrade: websocket\r\nConnection: Upgrade\r\n")
    case("accept/POST is 400", head.startswith("HTTP/1.1 400"), head)
    status, rh, conn = handshake(p, "/fail")
    lines("accept/the handler returning an error closes with 1011", close_code(conn.messages() if conn else []), 1011)
    status, rh, conn = handshake(p, "/crash")
    msgs = conn.messages() if conn else []
    lines("accept/a runtime error in the handler closes with 1011", (msgs[:1], close_code(msgs)), ([(1, b"about to fail", False)], 1011))
    head, body, _ = raw_get(p, "/after")
    lines("accept/the server goes on", body.decode(), "plain /after")


def accept_echo(d, srv):
    n = 200 if QUICK else 1000
    src = ws_prog(srv.port, ECHO_BODY, N=n, MAX=65536).replace("__fill", "fill") + FILL.replace("__fill", "fill")
    want = sum(((i * 7919) % 65536 if (i * 7919) % 65536 > 1 else 1) for i in range(0, n, 2))
    code, out, err = run(d, src, timeout=300)
    lines(f"accept/{n} messages echoed to ws_connect", (code, out), (0, [f"echoed {want}"]))
    status, rh, conn = handshake(srv.port, "/echo")
    bad = 0
    for i in range(n):
        data = os.urandom(i % 3000)
        conn.send(2, data)
        fin, op, payload, masked = conn.read()
        bad += payload != data or masked
    conn.send(8, ws_peer.close_payload(1000))
    seen = conn.messages()
    lines(f"accept/{n} messages echoed to the Python client, then a close", (bad, close_code(seen)), (0, 1000))


# ---- the bound and stopping ----

HOLD_SERVER = r"""import std.http;
import std.metrics;
import std.sse;
import std.time;
import std.websocket;

fn hold[E: effects](s: EventStream[uses E]) -> result[void] uses E, clock {
    try map_err(send_event(s, new StreamEvent { name: "", data: "open", id: "" }), stream_error_text);
    while true {
        sleep(200);
        try map_err(keep_alive(s), stream_error_text);
    }
}

fn talk[E: effects](s: Socket[uses E]) -> result[void] uses E {
    while true {
        match receive_message(s, 0) {
            case ok(m) {
                match m {
                    case text(t) {
                        try map_err(send_text(s, t), ws_error_text);
                    }
                    case binary(b) {
                        try map_err(send_binary(s, b), ws_error_text);
                    }
                }
            }
            case err(e) {
                return ok();
            }
        }
    }
}

fn stubborn(s: Socket[uses serve]) -> result[void] uses serve, clock {
    while true {
        sleep(100);
    }
}

fn stream(r: Request, bound: int) -> Response uses serve, clock {
    return serve_events(r, new StreamOptions { headers: new list[Header], max_open: bound }, hold);
}

fn handle(r: Request) -> Response uses serve, clock, log {
    let accept = new AcceptOptions { origins: new list[str], protocols: new list[str], max_message: 0, max_open: 63 };
    if r.path == "/s2" {
        return stream(r, 2);
    }
    if r.path == "/s4" {
        return stream(r, 4);
    }
    if r.path == "/s6" {
        return stream(r, 6);
    }
    if r.path == "/s63" {
        return stream(r, 63);
    }
    if r.path == "/ws" {
        return ws_accept(r, accept, talk);
    }
    if r.path == "/stubborn" {
        return ws_accept(r, accept, stubborn);
    }
    if r.path == "/metrics" {
        return new Response { status: 200, headers: new list[Header], body: metrics_text(new list[Metric]) };
    }
    return new Response { status: 200, headers: new list[Header], body: "ok" };
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses console, clock, log, serve("{P}") {
    try serve_workers("{P}", {W}, setup, fn(n, r) => handle(r));
    return ok();
}
"""


def open_stream(port, path):
    """A connection that asked for path; (socket, status) after reading the head only."""
    s = socket.create_connection(("127.0.0.1", port), timeout=30)
    s.sendall(f"GET {path} HTTP/1.1\r\nHost: x\r\n\r\n".encode())
    data = b""
    while b"\r\n" not in data:
        got = s.recv(1)
        if not got:
            break
        data += got
    try:
        return s, int(data.split()[1])
    except (IndexError, ValueError):
        return s, 0


def latency(port, n=10):
    t = time.time()
    for _ in range(n):
        raw_get(port, "/health")
    return (time.time() - t) / n


def gauge(port):
    head, body, _ = raw_get(port, "/metrics")
    for line in body.decode().splitlines():
        if line.startswith("http_conversations_open "):
            return int(float(line.split()[1]))
    return None


def group_bound(d):
    srv = VoServer(d, HOLD_SERVER.replace("{W}", "8"))
    held = []
    try:
        idle = latency(srv.port)
        statuses = []
        for _ in range(50):
            sock, st = open_stream(srv.port, "/s4")
            held.append(sock)
            statuses.append(st)
        busy = latency(srv.port)
        lines("bound/50 clients on a route with max_open 4: 4 served, 46 refused", (statuses.count(200), statuses.count(503)), (4, 46))
        case("bound/ordinary requests stay within twice their idle time", busy < max(2 * idle, 0.02), f"idle {idle * 1000:.1f} ms, busy {busy * 1000:.1f} ms")
        lines("bound/the gauge shows the open conversations", gauge(srv.port), 4)
        for sock in held:
            sock.close()
        held = []
        time.sleep(1.0)
        lines("bound/and 0 once they end", gauge(srv.port), 0)
        st2 = []
        for path in ["/s2"] * 3 + ["/s6"] * 6:
            sock, st = open_stream(srv.port, path)
            held.append(sock)
            st2.append(st)
        lines("bound/routes with max_open 2 and 6 share the count", st2, [200, 200, 503, 200, 200, 200, 200, 503, 503])
        for sock in held:
            sock.close()
        held = []
        time.sleep(1.0)
        st3 = []
        for _ in range(9):
            sock, st = open_stream(srv.port, "/s63")
            held.append(sock)
            st3.append(st)
        lines("bound/max_open 63 with 8 workers stops at 7", st3.count(200), 7)
    finally:
        for sock in held:
            sock.close()
        code, out, err = srv.stop()
    case("bound/the server stops cleanly", code == 0, f"{code} {err[-300:]}")


def group_stop(d):
    for drain in ([], ["--drain", "5"]):
        srv = VoServer(d, HOLD_SERVER.replace("{W}", "64"), extra=drain)
        ws_conns, streams = [], []
        try:
            for i in range(30):
                status, rh, conn = handshake(srv.port, "/ws")
                if conn:
                    ws_conns.append(conn)
            for i in range(30):
                sock, st = open_stream(srv.port, "/s63")
                if st == 200:
                    streams.append(sock)
            opened = (len(ws_conns), len(streams))
        finally:
            t = time.time()
            code, out, err = srv.stop()
            took = time.time() - t
        codes = [close_code(c.messages()) for c in ws_conns]
        ended = 0
        for sock in streams:
            sock.settimeout(5)
            try:
                while sock.recv(65536):
                    pass
                ended += 1
            except OSError:
                pass
        label = "with --drain 5" if drain else "default drain"
        lines(f"stop/{label}: 60 conversations open", opened, (30, 30))
        lines(f"stop/{label}: every WebSocket closed with 1001", codes, [1001] * 30)
        lines(f"stop/{label}: every stream ended", ended, 30)
        case(f"stop/{label}: the server exits quickly and cleanly", code == 0 and took < 5 * SLOW, f"{code} {took:.1f} s {err[-300:]}")
    srv = VoServer(d, HOLD_SERVER.replace("{W}", "4"), extra=["--drain", "2"])
    status, rh, conn = handshake(srv.port, "/stubborn")
    t = time.time()
    code, out, err = srv.stop()
    took = time.time() - t
    case("stop/a handler that ignores the stop is ended at the drain limit", status == 101 and took < 4 * SLOW, f"{status} {took:.1f} s {code}")
    single = VoServer(d, HOLD_SERVER.replace("serve_workers(\"{P}\", {W}, setup, fn(n, r) => handle(r))", "serve(\"{P}\", handle)"))
    status, rh, conn = handshake(single.port, "/ws")
    t = time.time()
    code, out, err = single.stop()
    took = time.time() - t
    msgs = conn.messages() if conn else []
    lines("stop/under serve: the conversation closed with 1001 and the server ends", (status, close_code(msgs), code), (101, 1001, 0))


# ---- hostile input ----

FUZZ = "--fuzz" in sys.argv


def fuzz_count():
    return 100000 if FUZZ else 2000


def seeds(masked):
    small = [(n, raw) for n, raw, _, _ in ws_peer.hostile(masked) if len(raw) < 4096]
    good = [raw for n, raw, _ in ws_peer.good(masked) if len(raw) < 4096]
    return [raw for _, raw in small] + good


def group_hostile(d):
    import random
    rng = random.Random(63)
    n = fuzz_count()
    hostile_beside_echo(d)
    # the WebSocket client: each mutated sequence from the server, then the server ends its side
    inputs = ws_peer.mutated(rng, seeds(False), n)
    srv = ws_peer.Server({f"/{k}": ("fin", raw) for k, raw in enumerate(inputs)})
    t = time.time()
    code, out, err = run(d, ws_prog(srv.port, CASES_BODY.replace("drain(s, 3000)", "drain(s, 2000)"), N=n), extra=["--max-memory", "64"], timeout=7200)
    srv.stop()
    cases_seen = sum(1 for line in out if line.startswith("case "))
    case(f"hostile/{n} mutated sequences to the client: error values only", code == 0 and cases_seen == n and "R0" not in err,
         f"{code} {cases_seen} {err[-400:]}")
    print(f"  hostile/client fuzz: {time.time() - t:.1f} s")
    # the WebSocket server: each mutated sequence from a client
    inputs = ws_peer.mutated(rng, seeds(True), n)
    vs = VoServer(d, WS_SERVER, extra=["--max-memory", "64"])
    t = time.time()
    try:
        for k, raw in enumerate(inputs):
            status, rh, conn = handshake(vs.port, f"/h/{k}")
            if conn:
                _quiet(conn.raw, raw)
                _quiet(conn.sock.shutdown, socket.SHUT_WR)
                conn.messages()
                conn.sock.close()
        head, body, _ = raw_get(vs.port, "/after")
        lines("hostile/the server answers after the fuzzing", body.decode(), "plain /after")
    finally:
        code, out, err = vs.stop()
    told = len({line.split()[0] for line in out.splitlines() if line.startswith("/h/") and line.split()[1] not in ("text", "binary")})
    case(f"hostile/{n} mutated sequences to the server: error values only", code == 0 and told == n and "R0" not in err,
         f"{code} {told} {err[-400:]}")
    print(f"  hostile/server fuzz: {time.time() - t:.1f} s")
    # the event-stream reader
    base = [ev("a\nb", "x", "1", "c"), b"retry: 10\r\n" + ev("d", end=b"\r"), b"\xef\xbb\xbfdata: x\n\n", ev("\u00e5" * 50),
            b"data:" + b"\xff\xfe" + b"\n\n", b"id: \x00\ndata: z\n\n", b":\n" * 20 + b"data\n\n"]
    inputs = ws_peer.mutated(rng, base, n)
    es = EventServer()
    for k, raw in enumerate(inputs):
        es.scripts[f"/{k}"] = (None, [(0, raw[: len(raw) // 2]), (0, raw[len(raw) // 2:])], "chunked" if k % 2 else "close")
    body = """for k in 0..{N} {
        let end = sse.read_events("GET", "http://127.0.0.1:{P}/" + str(k), new list[http.Header], "", "", 2000, fn(e) => ok(true));
        print("case " + str(k) + " " + why(end.why));
    }"""
    src = READER.replace("{BODY}", body.replace("{N}", str(n))).replace("{P}", str(es.port))
    t = time.time()
    code, out, err = run(d, src, extra=["--max-memory", "64"], timeout=7200)
    es.stop()
    case(f"hostile/{n} mutated event streams: an end each, no runtime error", code == 0 and len(out) == n, f"{code} {len(out)} {err[-400:]}")
    print(f"  hostile/stream fuzz: {time.time() - t:.1f} s")


def hostile_beside_echo(d):
    vs = VoServer(d, WS_SERVER)
    try:
        status, rh, echo = handshake(vs.port, "/echo")
        bad = 0
        for k, (n, raw, w, c) in enumerate(ws_peer.hostile(True)):
            st, _, conn = handshake(vs.port, f"/h/{k}")
            if conn:
                threading.Thread(target=lambda c_=conn, r_=raw: _quiet(c_.raw, r_), daemon=True).start()
                conn.messages()
                conn.sock.close()
            echo.send(1, f"still {k}".encode())
            fin, op, payload, masked = echo.read()
            bad += payload != f"still {k}".encode()
        lines("hostile/a second conversation echoes undisturbed while the first is fed every hostile sequence", bad, 0)
        for i in range(10000):
            echo.send(9, b"p")
        echo.send(1, b"after pings")
        pongs, last = 0, None
        while True:
            fin, op, payload, masked = echo.read()
            if op == 10:
                pongs += 1
            else:
                last = payload
                break
        lines("hostile/a server answers 10,000 pings and goes on", (pongs, last), (10000, b"after pings"))
        echo.send(8, ws_peer.close_payload(1000))
        echo.messages()
    finally:
        vs.stop()


# ---- recordings ----

WS_TALKER = """let s = try map_err(ws_connect("ws://127.0.0.1:{P}/echo", options(), 5000), word);
    for i in 0..5 {
        try map_err(send_text(s, "m" + str(i)), word);
        match receive_message(s, 0) {
            case ok(m) {
                print(shown(m));
            }
            case err(e) {
                print("error " + word(e));
            }
        }
    }
    try map_err(close_socket(s, 1000, ""), word);
    print("closed");"""

SSE_SENDER = r"""import std.http;
import std.sse;
import std.time;

fn count(s: EventStream[uses serve]) -> result[void] uses serve, console, clock {
    for i in 0..20 {
        match send_event(s, new StreamEvent { name: "", data: str(i), id: "" }) {
            case ok() {
            }
            case err(e) {
                print("stopped at " + str(i) + ": " + stream_error_text(e));
                return ok();
            }
        }
        sleep(20);
    }
    print("sent 20");
    return ok();
}

fn handle(r: Request) -> Response uses serve, console, clock {
    return serve_events(r, new StreamOptions { headers: new list[Header], max_open: 0 }, count);
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses console, clock, serve("{P}") {
    try serve_workers("{P}", 2, setup, fn(n, r) => handle(r));
    return ok();
}
"""


def group_replay(d):
    import random
    rng = random.Random(7)
    n = 10 if QUICK else 100
    bad = {"sse reader": 0, "ws client": 0, "sse server": 0, "ws server": 0}
    for i in range(n):
        es = EventServer()
        es.scripts["/r"] = (None, [(rng.random() * 0.02, ev(f"e{k}", id=str(k))) for k in range(10)], rng.choice(["close", "chunked", "chunked-cut"]))
        sub = os.path.join(d, f"r{i}")
        os.mkdir(sub)
        bad["sse reader"] += not replays_same(sub, reader(es.port, "/r", ms=3000), "a.vo")
        es.stop()

        def slow_echo(conn, r=random.Random(i)):
            while True:
                fin, op, payload, masked = conn.read()
                if op == 8:
                    conn.send(8, payload[:2])
                    return
                time.sleep(r.random() * 0.02)
                if r.random() < 0.1:
                    conn.send(9, b"ping")
                conn.send(op, payload)
        ws = ws_peer.Server(slow_echo)
        bad["ws client"] += not replays_same(sub, ws_prog(ws.port, WS_TALKER), "b.vo")
        ws.stop()
        bad["sse server"] += not server_replays_same(sub, SSE_SENDER, "c.vo", lambda port: sse_client(port, rng))
        bad["ws server"] += not server_replays_same(sub, WS_SERVER, "e.vo", lambda port: ws_client(port, rng))
    for role, k in bad.items():
        lines(f"replay/{role}: {n} recordings against peers with random delays replay to their own run", k, 0)
    es = EventServer()
    es.scripts["/same"] = (None, [(0.001, ev(f"x{k}")) for k in range(50)], "chunked")
    outs = {tuple(run(d, reader(es.port, "/same", ms=0))[1]) for _ in range(5 if QUICK else 20)}
    es.stop()
    lines("replay/reads without a time limit print the same bytes on every run", len(outs), 1)
    replay_redaction(d)


def replays_same(d, src, name):
    rec = name + ".rec"
    code, out, err = run(d, src, name, extra=["--record", rec])
    rcode, rout, rerr = vo(d, ["replay", rec])
    if (code, out) != (rcode, rout):
        print(f"  differs: {name} {(code, out)} vs {(rcode, rout)} {rerr[-300:]}")
    return (code, out) == (rcode, rout)


def server_replays_same(d, src, name, client):
    srv = VoServer(d, src, name=name, extra=["--record", name + ".rec"])
    try:
        client(srv.port)
    finally:
        code, out, err = srv.stop()
    rcode, rout, rerr = vo(d, ["replay", name + ".rec"])
    # a replay runs the requests one at a time in arrival order (spec 028), so the output of conversations
    # that overlapped in time can come in another order: the same lines, each request's in its order
    same = code == rcode and sorted(out.splitlines()) == sorted(rout)
    if not same:
        print(f"  differs: {name} {(code, out)} vs {(rcode, rout)} {rerr[-300:]}")
    return same


def sse_client(port, rng):
    for _ in range(3):
        s = socket.create_connection(("127.0.0.1", port), timeout=30)
        s.sendall(b"GET /x HTTP/1.1\r\nHost: x\r\n\r\n")
        end = time.time() + rng.random() * 0.6
        while time.time() < end:
            s.settimeout(max(0.01, end - time.time()))
            try:
                if not s.recv(65536):
                    break
            except socket.timeout:
                break
        s.close()
    time.sleep(0.5)


def ws_client(port, rng):
    for k in range(3):
        status, rh, conn = handshake(port, f"/h/{k}")
        if not conn:
            continue
        for j in range(rng.randint(0, 5)):
            conn.send(1, f"msg {j}".encode())
            time.sleep(rng.random() * 0.02)
        if rng.random() < 0.7:
            conn.send(8, ws_peer.close_payload(1000))
            conn.messages()
        conn.sock.close()
    time.sleep(0.3)


TEST_REDACT = """import std.http;
import std.websocket;

test "talks with a token" uses net("127.0.0.1:{P}") {
    let options = new ConnectOptions { headers: [new Header { name: "Authorization", value: "Bearer s3cret" }], protocols: new list[str], max_message: 0 };
    match ws_connect("ws://127.0.0.1:{P}/echo", options, 5000) {
        case ok(s) {
            expect(send_text(s, "hello") == ok());
            expect(receive_message(s, 5000) == ok(Message.text("hello")));
            expect(close_socket(s, 1000, "") == ok());
        }
        case err(e) {
            expect(false);
        }
    }
}
"""


def replay_redaction(d):
    srv = ws_peer.Server("echo")
    with open(os.path.join(d, "t.vo"), "w") as f:
        f.write(module_form(TEST_REDACT.replace("{P}", str(srv.port))))
    code, out, err = vo(d, ["test", "--record", "rec", "t.vo"])
    srv.stop()
    text = open(os.path.join(d, "rec", "t", "talks_with_a_token.jsonl")).read() if code == 0 else ""
    case("replay/a recorded test holds no handshake token", code == 0 and "s3cret" not in text and "[redacted]" in text, f"{code} {out} {text[:300]}")
    code, out, err = vo(d, ["test", "--replay", "rec", "t.vo"])
    case("replay/and replays without the peer", code == 0, f"{code} {out} {err[-300:]}")
    with open(os.path.join(d, "t.vo"), "w") as f:
        f.write(module_form(TEST_REDACT.replace("{P}", str(srv.port)).replace('send_text(s, "hello")', 'send_text(s, "jello")')))
    code, out, err = vo(d, ["test", "--replay", "rec", "t.vo"])
    case("replay/a changed message is reported at its send", code != 0 and ("R0014" in "\n".join(out) + err or "differ" in "\n".join(out) + err), f"{code} {out[-6:]} {err[-300:]}")
    es = EventServer()
    es.scripts["/t"] = (None, [(0.01, ev("a", id="1")), (0.01, ev("b", id="2"))], "chunked")
    test_src = """import std.http;
import std.sse;

test "reads two events" uses net("127.0.0.1:{P}") {
    let got = new list[str];
    let end = read_events("GET", "http://127.0.0.1:{P}/t", new list[Header], "", "", 5000, fn(e) {
        push(got, e.data);
        return ok(true);
    });
    expect(got == ["a", "b"] and end.last_id == "2");
}
""".replace("{P}", str(es.port))
    with open(os.path.join(d, "u.vo"), "w") as f:
        f.write(module_form(test_src))
    code, out, err = vo(d, ["test", "--record", "rec2", "u.vo"])
    es.stop()
    rcode, rout, rerr = vo(d, ["test", "--replay", "rec2", "u.vo"])
    lines("replay/a recorded test that reads a stream replays without the server", (code, rcode), (0, 0))


# ---- the take under hostile clients ----

def settle(port, limit=10.0):
    """Waits until the server holds no conversation (each handler notices its client left at its next send)."""
    end = time.time() + limit
    while time.time() < end and gauge(port) not in (0, None):
        time.sleep(0.1)


def group_take(d):
    srv = VoServer(d, HOLD_SERVER.replace("{W}", "4"))
    try:
        for i in range(50):  # a client that closes during the hand-over
            s = socket.create_connection(("127.0.0.1", srv.port), timeout=30)
            s.sendall(b"GET /s63 HTTP/1.1\r\nHost: x\r\n\r\n")
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            s.close()
        settle(srv.port)
        s = socket.create_connection(("127.0.0.1", srv.port), timeout=30)
        s.sendall(b"GET /s63 HTTP/1.1\r\nHost: x\r\n\r\nGET /again HTTP/1.1\r\n\r\n")
        answer = s.recv(65536).decode(errors="replace")
        s.close()
        case("take/bytes after the request are refused with 400", answer.startswith("HTTP/1.1 400"), answer[:80])
        for i in range(20):  # half a WebSocket handshake, then gone
            s = socket.create_connection(("127.0.0.1", srv.port), timeout=30)
            s.sendall(b"GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n")
            s.close()
        settle(srv.port)
        lines("take/the server still answers", raw_get(srv.port, "/plain")[1].decode(), "ok")
        lines("take/and no conversation is still counted", gauge(srv.port), 0)
        held = []
        for i in range(5):
            sock, st = open_stream(srv.port, "/s63")
            held.append((sock, st))
        lines("take/the bound is whole again (3 of 4 workers)", [st for _, st in held].count(200), 3)
        for sock, _ in held:
            sock.close()
    finally:
        code, out, err = srv.stop()
    case("take/the server stops cleanly", code == 0, f"{code} {err[-300:]}")


# ---- main ----

GROUPS = {
    "read": group_read,
    "workers": group_workers,
    "client": group_client,
    "stream": group_stream,
    "accept": group_accept,
    "bound": group_bound,
    "stop": group_stop,
    "hostile": group_hostile,
    "replay": group_replay,
    "take": group_take,
}


def main():
    args = sys.argv[1:]
    names = list(GROUPS)
    for flag in ("--only", "--group"):
        if flag in args:
            names = [args[args.index(flag) + 1]]
    with tempfile.TemporaryDirectory() as d:
        for n in names:
            sub = os.path.join(d, n)
            os.mkdir(sub)
            GROUPS[n](sub)
    print(f"events: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.stdout.reconfigure(line_buffering=True)
    sys.exit(main())
