#!/usr/bin/env python3
"""Spec 039: std.tcp against local peers (tests/tcp_server.py).

  tcp.py                 every group (part of `make test`)
  tcp.py --group NAME    one group: client, limits, scopes, tls, server, tasks, replay, docs
  tcp.py --time          SC-005: a 100-byte round trip and 16 MiB on loopback (timing; not a pass/fail)

Each case writes a small vo program with the peer's port in its `net` or `serve` scope, runs it with
bin/vo ($VO_BIN; under qemu for the other architectures) and compares what it prints.
"""
import os
import re
import socket
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1  # under qemu
CA = os.path.join(ROOT, "tests", "tls", "certs", "test-ca.pem")
SERVER = os.path.join(ROOT, "tests", "tcp_server.py")
failures = []
cases = 0

PRELUDE = """import std.bytes.from_ints;
import std.bytes.from_text;
import std.bytes.to_text;
import std.tcp.Conn;
import std.tcp.Listener;
import std.tcp.TcpError;
import std.tcp.accept;
import std.tcp.close_conn;
import std.tcp.close_listener;
import std.tcp.connect;
import std.tcp.connect_tls;
import std.tcp.tcp_error_text;
import std.tcp.listen;
import std.tcp.read;
import std.tcp.read_exact;
import std.tcp.read_until;
import std.tcp.write_all;
import std.tcp.start_tls;

fn text(b: bytes) -> str {
    match to_text(b) {
        case ok(t) {
            return trim(t);
        }
        case err(_) {
            return "(not text)";
        }
    }
}

fn show(r: result[bytes, TcpError]) -> str {
    match r {
        case ok(b) {
            return "ok " + str(len(b)) + " " + text(b);
        }
        case err(e) {
            return "err " + tcp_error_text(e);
        }
    }
}

fn did(r: result[void, TcpError]) -> str {
    match r {
        case ok() {
            return "ok";
        }
        case err(e) {
            return "err " + tcp_error_text(e);
        }
    }
}

fn opened[E: effects](r: result[Conn[E], TcpError]) -> str {
    match r {
        case ok(_) {
            return "ok";
        }
        case err(e) {
            return "err " + tcp_error_text(e);
        }
    }
}
"""

# A client: `talk` holds the conversation on one connection to 127.0.0.1:P.
CLIENT = PRELUDE + """
fn talk(c: Conn[uses net]) uses console, net("127.0.0.1:{P}") {
{BODY}
}

fn main() uses console, net("127.0.0.1:{P}") {
    match {CONNECT}("127.0.0.1", {P}, {MS}) {
        case ok(c) {
            talk(c);
        }
        case err(e) {
            print("connect: " + tcp_error_text(e));
        }
    }
}
"""

BASIC = """    print(did(write_all(c, from_text("one\\n"), 2000)));
    print(show(read_until(c, from_text("\\n"), 2000)));
    print(did(write_all(c, from_text("abcdef"), 2000)));
    print(show(read_exact(c, 3, 2000)));
    print(show(read(c, 100, 2000)));
    print(did(write_all(c, from_text("x--y--z"), 2000)));
    print(show(read_until(c, from_text("--"), 2000)));
    print(show(read_until(c, from_text("--"), 2000)));
    print(show(read(c, 10, 2000)));
    print(str(c.peer == "127.0.0.1:{P}"));
    close_conn(c);
    print(did(write_all(c, from_text("late"), 2000)));
    print(show(read(c, 10, 2000)));
    close_conn(c);"""
BASIC_OUT = ["ok", "ok 4 one", "ok", "ok 3 abc", "ok 3 def", "ok", "ok 3 x--", "ok 3 y--", "ok 1 z", "true",
             "err the connection is closed", "err the connection is closed"]

THOUSAND = """    var bad = 0;
    for i in 0..1000 {
        let m = from_text("message " + str(i) + "\\n");
        match write_all(c, m, 2000) {
            case ok() {
            }
            case err(_) {
                set bad = bad + 1;
            }
        }
        match read_exact(c, len(m), 2000) {
            case ok(b) {
                if b != m {
                    set bad = bad + 1;
                }
            }
            case err(_) {
                set bad = bad + 1;
            }
        }
    }
    var xs = new list[int];
    for k in 0..256 {
        push(xs, k);
    }
    let every = from_ints(xs);
    print(did(write_all(c, every, 2000)));
    match read_exact(c, 256, 2000) {
        case ok(b) {
            print(str(b == every));
        }
        case err(e) {
            print(tcp_error_text(e));
        }
    }
    print(str(bad));
    close_conn(c);"""

SILENT = """    print(show(read(c, 10, {WAIT})));
    print(show(read_until(c, from_text("\\n"), {WAIT})));
    print(did(write_all(c, from_text("still here"), 2000)));
    close_conn(c);"""
SILENT_OUT = ["err the time limit passed", "err the time limit passed", "ok"]

HALF = """    print(show(read_until(c, from_text("\\n"), 2000)));
    print(show(read_exact(c, 10, 2000)));
    print(show(read(c, 10, 2000)));
    print(show(read(c, 10, 2000)));
    print(did(write_all(c, from_text("x"), 2000)));
    close_conn(c);"""
HALF_OUT = ["err the connection is closed", "err the connection is closed", "ok 3 par", "err the connection is closed",
            "err the connection is closed"]

RESET = """    print(did(write_all(c, from_text("a"), 2000)));
    print(show(read(c, 10, 2000)));
    print(did(write_all(c, from_text("b"), 2000)));
    print("still running");"""
RESET_OUT = ["ok", "err the connection is closed", "err the connection is closed", "still running"]

BURST = """    match read_until(c, from_text("\\n"), 20000) {
        case ok(b) {
            print("ok " + str(len(b)));
        }
        case err(e) {
            print(tcp_error_text(e));
        }
    }
    match read(c, 100, 2000) {
        case ok(b) {
            print("still " + str(len(b)));
        }
        case err(e) {
            print(tcp_error_text(e));
        }
    }
    close_conn(c);"""

MANY = """    var conns: list[Conn[uses net]] = [c];
    for i in 1..256 {
        match connect("127.0.0.1", {P}, 5000) {
            case ok(more) {
                push(conns, more);
            }
            case err(e) {
                print("early: " + tcp_error_text(e));
            }
        }
    }
    print(str(len(conns)));
    print(opened(connect("127.0.0.1", {P}, 5000)));
    close_conn(conns[0]);
    print(opened(connect("127.0.0.1", {P}, 5000)));
    print(did(write_all(conns[255], from_text("last\\n"), 2000)));
    print(show(read_until(conns[255], from_text("\\n"), 2000)));"""

TLS_TALK = """    print(did(write_all(c, from_text("secret word\\n"), 3000)));
    print(show(read_until(c, from_text("\\n"), 3000)));
    print(did(start_tls(c, "127.0.0.1", 3000)));
    close_conn(c);"""
TLS_OUT = ["ok", "ok 12 secret word", "err invalid: the connection is already TLS"]

STARTTLS = """    print(show(read_until(c, from_text("\\n"), 3000)));
    print(did(write_all(c, from_text("STARTTLS\\n"), 3000)));
    print(show(read_until(c, from_text("\\n"), 3000)));
    print(did(start_tls(c, "127.0.0.1", {MS})));
    print(did(write_all(c, from_text("hidden word\\n"), 3000)));
    print(show(read_until(c, from_text("\\n"), 3000)));
    close_conn(c);"""

UNREAD = """    print(show(read_exact(c, 1, 3000)));
    print(did(start_tls(c, "127.0.0.1", 3000)));
    print(show(read_until(c, from_text("\\n"), 3000)));
    close_conn(c);"""

SERVER_VO = PRELUDE + """
fn echo[E: effects](c: Conn[E]) uses E {
    var open = true;
    while open {
        match read_until(c, from_text("\\n"), 5000) {
            case ok(line) {
                match write_all(c, line, 5000) {
                    case ok() {
                    }
                    case err(_) {
                        set open = false;
                    }
                }
            }
            case err(_) {
                set open = false;
            }
        }
    }
    close_conn(c);
}

fn main() uses console, serve("{P}") {
    match listen("{P}") {
        case ok(l) {
            print(str(l.port == {P}));
            for i in 0..2 {
                match accept(l, 10000) {
                    case ok(c) {
                        print(slice(c.peer, 0, 10));
                        echo(c);
                    }
                    case err(e) {
                        print("accept: " + tcp_error_text(e));
                    }
                }
            }
            close_listener(l);
        }
        case err(e) {
            print("listen: " + tcp_error_text(e));
        }
    }
}
"""

BOTH = PRELUDE + """
fn serve_one(l: Listener) -> str uses serve("0") {
    match accept(l, 5000) {
        case ok(c) {
            let line = read_until(c, from_text("\\n"), 5000);
            match line {
                case ok(b) {
                    let sent = write_all(c, b, 5000);
                    close_conn(c);
                    return "served " + text(b) + " " + did(sent);
                }
                case err(e) {
                    return "server read: " + tcp_error_text(e);
                }
            }
        }
        case err(e) {
            return "accept: " + tcp_error_text(e);
        }
    }
}

fn ask(port: int) -> str uses net("127.0.0.1") {
    match connect("127.0.0.1", port, 5000) {
        case ok(c) {
            let sent = write_all(c, from_text("ping\\n"), 5000);
            let back = read_until(c, from_text("\\n"), 5000);
            close_conn(c);
            return "asked " + did(sent) + ", got " + show(back);
        }
        case err(e) {
            return "connect: " + tcp_error_text(e);
        }
    }
}

fn both() -> list[str] uses net("127.0.0.1"), serve("0") {
    match listen("0") {
        case ok(l) {
            let out = concurrent_map([0, 1], 2, fn(role: int) -> str {
                if role == 0 {
                    return serve_one(l);
                }
                return ask(l.port);
            });
            close_listener(l);
            return out;
        }
        case err(e) {
            return ["listen: " + tcp_error_text(e)];
        }
    }
}

test "both ends in one program" uses net("127.0.0.1"), serve("0") {
    expect_eq(both(), ["served ping ok", "asked ok, got ok 5 ping"]);
}

fn main() uses console, net("127.0.0.1"), serve("0") {
    for line in both() {
        print(line);
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
    case(name, got == want, f"got {got!r}, want {want!r}")


class Peer:
    """A tests/tcp_server.py process; .port once it listens."""

    def __init__(self, mode, cert=None):
        cmd = [sys.executable, SERVER, mode] + (["--cert", cert] if cert else [])
        self.p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
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


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def client(body, port, connect="connect", ms=2000, **more):
    src = CLIENT.replace("{BODY}", body).replace("{CONNECT}", connect).replace("{MS}", str(ms))
    for k, v in more.items():
        src = src.replace("{" + k + "}", str(v))
    return src.replace("{P}", str(port))


def vo(d, words, timeout=60):
    r = subprocess.run([VO] + words, cwd=d, capture_output=True, text=True, timeout=timeout * SLOW)
    return r.returncode, r.stdout.splitlines(), r.stderr


def run(d, src, name="p.vo", extra=(), timeout=60):
    with open(os.path.join(d, name), "w") as f:
        f.write(src)
    return vo(d, ["run", "--timeout", str(30 * SLOW)] + list(extra) + [name], timeout)


def group_client(d):
    with Peer("echo") as e:
        code, out, err = run(d, client(BASIC, e.port))
        lines("client/basic", (code, out), (0, BASIC_OUT))
        code, out, err = run(d, client(THOUSAND, e.port))
        lines("client/thousand messages and every byte value (SC-001)", (code, out), (0, ["ok", "true", "0"]))
    code, out, err = run(d, client("    close_conn(c);", free_port()))
    lines("client/refused", (code, out), (0, ["connect: the connection was refused"]))
    with Peer("silent") as s:
        code, out, err = run(d, client(SILENT, s.port, WAIT=200 * SLOW))
        lines("client/timed out, then usable", (code, out), (0, SILENT_OUT))
    with Peer("half") as h:
        code, out, err = run(d, client(HALF, h.port))
        lines("client/peer closes: buffered bytes, then closed", (code, out), (0, HALF_OUT))
    with Peer("reset") as r:
        code, out, err = run(d, client(RESET, r.port))
        lines("client/peer resets", (code, out), (0, RESET_OUT))
    src = client("    close_conn(c);", 80).replace('("127.0.0.1", 80,', '("no-such-host.invalid", 80,')
    src = src.replace('net("127.0.0.1:80")', 'net("no-such-host.invalid")').replace("{MS}", "8000")
    code, out, err = run(d, src)
    # with a name server: not found; without one (a sandbox), the lookup gives up the same way
    case("client/unknown name", code == 0 and out in (["connect: the host was not found"], ["connect: the time limit passed"]),
         f"{code} {out} {err[:200]}")


def group_limits(d):
    with Peer("burst") as b:
        code, out, err = run(d, client(BURST, b.port), timeout=120)
        lines("limits/16 MiB without the delimiter", (code, out),
              (0, ["more than 16 MiB arrived without what the read asked for", "still 100"]))
    with Peer("echo") as e:
        code, out, err = run(d, client(MANY, e.port), timeout=120)
        lines("limits/256 open", (code, out),
              (0, ["256", "err too many connections are open", "ok", "ok", "ok 5 last"]))


def denied(name, code, err, target):
    case(name, code != 0 and "R0010" in err and target in err, f"{code} {err[:300]}")


def group_scopes(d):
    with Peer("echo") as e:
        watch = socket.socket()  # a port the program must never reach
        watch.bind(("127.0.0.1", 0))
        watch.listen(4)
        watch.settimeout(0.3)
        q = watch.getsockname()[1]
        src = client(BASIC, e.port).replace(f'("127.0.0.1", {e.port}, 2000)', f'("127.0.0.1", {q}, 2000)')
        code, out, err = run(d, src)
        denied("scopes/another port", code, err, f'net("127.0.0.1:{q}")')
        src = client(BASIC, q).replace(f'net("127.0.0.1:{q}")', f'net("localhost:{q}")')
        code, out, err = run(d, src)
        denied("scopes/another host", code, err, f'net("127.0.0.1:{q}")')
        src = f"""import std.http.get;

fn main() uses console, net("127.0.0.1:{e.port}") {{
    match get("http://127.0.0.1:{q}/") {{
        case ok(r) {{
            print(str(r.status));
        }}
        case err(m) {{
            print(m);
        }}
    }}
}}
"""
        code, out, err = run(d, src)
        denied("scopes/std.http obeys a port", code, err, f'net("127.0.0.1:{q}")')
        try:
            watch.accept()
            case("scopes/nothing was opened outside the grant (SC-002)", False, "a connection arrived")
        except socket.timeout:
            case("scopes/nothing was opened outside the grant (SC-002)", True)
        watch.close()
        src = client(BASIC, e.port).replace(f'net("127.0.0.1:{e.port}")', 'net("127.0.0.1")')
        code, out, err = run(d, src)
        lines("scopes/a bare host allows any port", (code, out), (0, BASIC_OUT))
        forged = """    print(sneak(c.id));
    close_conn(c);
}

fn sneak(id: int) -> str {
    let forged = new Conn { id: id, peer: "" };
    return show(read(forged, 1, 500));"""
        code, out, err = run(d, client(forged, e.port))
        denied("scopes/a forged connection in a function without net", code, err, "net")
    p = free_port()
    src = SERVER_VO.replace('listen("{P}")', f'listen("{free_port()}")').replace("{P}", str(p))
    code, out, err = run(d, src)
    denied("scopes/listening on another address", code, err, "serve(")


def group_tls(d):
    ca = ["--ca-file", CA]
    with Peer("tls", "a.test") as t:
        code, out, err = run(d, client(TLS_TALK, t.port, "connect_tls", 5000 * SLOW), extra=ca)
        lines("tls/from the first byte", (code, out), (0, TLS_OUT))
    with Peer("starttls", "a.test") as t:
        code, out, err = run(d, client(STARTTLS, t.port, ms=5000 * SLOW), extra=ca)
        lines("tls/upgrade", (code, out),
              (0, ["ok 10 220 ready", "ok", "ok 7 220 go", "ok", "ok", "ok 12 hidden word"]))
    why = {"other-name": "is for another host", "expired": "has expired", "rogue": "is not trusted"}
    for cert, reason in why.items():
        with Peer("tls", cert) as t:
            code, out, err = run(d, client("    close_conn(c);", t.port, "connect_tls", 5000 * SLOW), extra=ca)
            lines(f"tls/{cert}", (code, out), (0, [f"connect: TLS failed: the certificate of 127.0.0.1:{t.port} {reason}"]))
        with Peer("starttls", cert) as t:
            code, out, err = run(d, client(STARTTLS, t.port, ms=5000 * SLOW), extra=ca)
            want = f"err TLS failed: the certificate of 127.0.0.1:{t.port} {reason}"
            case(f"tls/{cert} on upgrade", code == 0 and len(out) > 3 and out[3] == want, f"{code} {out}")
    with Peer("greet") as g:
        code, out, err = run(d, client(UNREAD, g.port))
        lines("tls/no upgrade over unread bytes", (code, out),
              (0, ["ok 1 h", "err invalid: bytes received before TLS are waiting to be read", "ok 5 ello"]))
    with Peer("silent") as s:
        code, out, err = run(d, client("    close_conn(c);", s.port, "connect_tls", 300 * SLOW), extra=ca)
        lines("tls/handshake time limit", (code, out), (0, ["connect: the time limit passed"]))


def converse(port, words):
    """Connects to a vo server (retrying until it listens) and returns what came back per line."""
    for _ in range(200 * SLOW):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=5 * SLOW)
            break
        except OSError:
            time.sleep(0.05)
    else:
        return None
    got = []
    f = s.makefile("rwb")
    for w in words:
        f.write(w + b"\n")
        f.flush()
        got.append(f.readline().rstrip(b"\n"))
    s.close()
    return got


def serve_run(d, words, name="server.vo"):
    """Runs the vo echo server with two clients in turn: (exit status, stdout lines, clients' replies)."""
    p = free_port()
    with open(os.path.join(d, name), "w") as f:
        f.write(SERVER_VO.replace("{P}", str(p)))
    srv = subprocess.Popen([VO] + words + [name], cwd=d, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    a = converse(p, [b"first", b"second"])
    b = converse(p, [b"third"])
    out, err = srv.communicate(timeout=60 * SLOW)
    return srv.returncode, out.splitlines(), (a, b)


def group_server(d):
    code, out, replies = serve_run(d, ["run", "--timeout", str(60 * SLOW)])
    lines("server/echo for two clients in turn", (code, out, replies),
          (0, ["true", "127.0.0.1:", "127.0.0.1:"], ([b"first", b"second"], [b"third"])))
    held = socket.socket()
    held.bind(("127.0.0.1", 0))
    held.listen(1)
    code, out, err = run(d, SERVER_VO.replace("{P}", str(held.getsockname()[1])))
    lines("server/port in use", (code, out), (0, ["listen: the port is already in use"]))
    held.close()
    src = SERVER_VO.replace("accept(l, 10000)", f"accept(l, {200 * SLOW})").replace("{P}", str(free_port()))
    code, out, err = run(d, src)
    lines("server/no client", (code, out), (0, ["true", "accept: the time limit passed", "accept: the time limit passed"]))


def group_tasks(d):
    with open(os.path.join(d, "both.vo"), "w") as f:
        f.write(BOTH)
    code, out, err = vo(d, ["test", "both.vo"])
    case("tasks/a test with both ends", code == 0, f"{code} {out} {err[:300]}")
    code, out, err = vo(d, ["run", "both.vo"])
    lines("tasks/run", (code, out), (0, ["served ping ok", "asked ok, got ok 5 ping"]))


def replayed(d, name, src, peer, want, extra=(), file="r.vo"):
    """Records the program against the peer, stops the peer, replays: the same output both times."""
    rec = file + ".rec"
    code, out, err = run(d, src, file, extra=["--record", rec] + list(extra))
    lines(f"replay/{name}: the recorded run", (code, out), (0, want))
    if peer:
        peer.stop()
    t = time.time()
    code, out, err = vo(d, ["replay", rec])
    took = time.time() - t
    lines(f"replay/{name}", (code, out), (0, want))
    return took, os.path.join(d, rec)


def group_replay(d):
    e = Peer("echo")
    replayed(d, "client", client(BASIC, e.port), e, BASIC_OUT)
    s = Peer("silent")
    took, _ = replayed(d, "timeouts", client(SILENT, s.port, WAIT=1500), s, SILENT_OUT)
    case("replay/a recorded timeout does not wait", took < 1.0 * SLOW, f"{took:.2f} s")
    h = Peer("half")
    replayed(d, "peer closes", client(HALF, h.port), h, HALF_OUT)
    r = Peer("reset")
    replayed(d, "failed writes", client(RESET, r.port), r, RESET_OUT)
    t = Peer("tls", "a.test")
    _, rec = replayed(d, "tls", client(TLS_TALK, t.port, "connect_tls", 5000 * SLOW), t, TLS_OUT, extra=["--ca-file", CA])
    text = open(rec, errors="replace").read()
    case("replay/a TLS recording holds the plain bytes and no key", "secret word" in text and "PRIVATE" not in text
         and '"kind":"tcp_open"' in text, text[:300])
    t = Peer("starttls", "a.test")
    replayed(d, "upgrade", client(STARTTLS, t.port, ms=5000 * SLOW), t,
             ["ok 10 220 ready", "ok", "ok 7 220 go", "ok", "ok", "ok 12 hidden word"], extra=["--ca-file", CA])
    t = Peer("tls", "expired")
    replayed(d, "a failed handshake", client("    close_conn(c);", t.port, "connect_tls", 5000 * SLOW), t,
             [f"connect: TLS failed: the certificate of 127.0.0.1:{t.port} has expired"], extra=["--ca-file", CA])
    replayed(d, "refused", client("    close_conn(c);", free_port()), None, ["connect: the connection was refused"])
    # a server: recorded with two clients, replayed with none and no port opened
    code, out, replies = serve_run(d, ["run", "--timeout", str(60 * SLOW), "--record", "server.rec"])
    want = ["true", "127.0.0.1:", "127.0.0.1:"]
    lines("replay/server: the recorded run", (code, out), (0, want))
    code, out, err = vo(d, ["replay", "server.rec"])
    lines("replay/server", (code, out), (0, want))
    with open(os.path.join(d, "both.vo"), "w") as f:
        f.write(BOTH)
    code, out, err = vo(d, ["run", "--record", "both.rec", "both.vo"])
    want = ["served ping ok", "asked ok, got ok 5 ping"]
    lines("replay/tasks: the recorded run", (code, out), (0, want))
    code, out, err = vo(d, ["replay", "both.rec"])
    lines("replay/tasks", (code, out), (0, want))
    # a changed program that connects elsewhere
    e = Peer("echo")
    src = client(BASIC, e.port).replace(f'net("127.0.0.1:{e.port}")', 'net("127.0.0.1")')
    replayed(d, "before a change", src, e, BASIC_OUT, file="c.vo")
    with open(os.path.join(d, "c.vo"), "w") as f:
        f.write(src.replace(f'("127.0.0.1", {e.port}, 2000)', f'("127.0.0.1", {e.port + 1 if e.port < 65535 else 1024}, 2000)'))
    code, out, err = vo(d, ["replay", "c.vo.rec"])
    case("replay/another port is R0014", code != 0 and "R0014" in err, f"{code} {err[:300]}")


def group_docs(d):
    page = os.path.join(ROOT, "site", "docs", "guide", "standard-library.md")
    if not os.path.exists(page):
        case("docs/the page exists", False, page)
        return
    m = re.search(r"## std\.tcp.*?```vo\n(.*?\n)```", open(page).read(), re.S)
    if not m:
        case("docs/the Redis example is in the page", False)
        return
    src = m.group(1)
    case("docs/the Redis client is at most 60 lines (SC-006)", len(src.splitlines()) <= 60, f"{len(src.splitlines())} lines")
    with Peer("redis") as r:
        code, out, err = run(d, src.replace("6379", str(r.port)))
        lines("docs/the Redis client works (SC-006)", (code, out), (0, ["PONG", "OK", "tea"]))


TIME_TRIPS = """    let m = from_text("{HUNDRED}");
    let start = now_ms();
    for i in 0..2000 {
        match write_all(c, m, 2000) {
            case ok() {
            }
            case err(e) {
                print(tcp_error_text(e));
            }
        }
        match read_exact(c, 100, 2000) {
            case ok(_) {
            }
            case err(e) {
                print(tcp_error_text(e));
            }
        }
    }
    print(str(now_ms() - start));
    close_conn(c);"""
TIME_BLOB = """    let start = now_ms();
    var total = 0;
    while total < 16777216 {
        match read(c, 1048576, 5000) {
            case ok(b) {
                set total = total + len(b);
            }
            case err(e) {
                print(tcp_error_text(e));
                set total = 16777216;
            }
        }
    }
    print(str(now_ms() - start));
    close_conn(c);"""


def timing(d):
    with Peer("echo") as e:
        src = client(TIME_TRIPS, e.port, HUNDRED="x" * 100).replace("uses console, net", "uses console, clock, net")
        code, out, err = run(d, src)
        print(f"tcp: 2000 round trips of 100 bytes: {out[-1] if out else err} ms ({int(out[-1]) / 2000:.3f} ms each)" if code == 0 and out
              else f"tcp: timing failed: {err[:200]}")
    with Peer("burst") as b:
        src = client(TIME_BLOB, b.port).replace("uses console, net", "uses console, clock, net")
        code, out, err = run(d, src)
        print(f"tcp: 16 MiB received in {out[-1] if out else err} ms")


GROUPS = {"client": group_client, "limits": group_limits, "scopes": group_scopes, "tls": group_tls,
          "server": group_server, "tasks": group_tasks, "replay": group_replay, "docs": group_docs}


def main():
    args = sys.argv[1:]
    with tempfile.TemporaryDirectory() as d:
        if "--time" in args:
            timing(d)
            return 0
        names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
        for n in names:
            sub = os.path.join(d, n)
            os.mkdir(sub)
            GROUPS[n](sub)
    print(f"tcp: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
