#!/usr/bin/env python3
"""Spec 013 (H8, H9, H10, L9, L11, M2): attacks on a vo server and a vo client that need timing, memory
or a hostile peer. After each attack the server must answer a normal request; exit 1 on any failure.
(H7 and M5 are step cases: tests/cases/security/h7_hostile_requests, m5_header_injection.)

  server_attacks.py [VO]
"""
import os, signal, socket, subprocess, sys, tempfile, threading, time

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
SERVER = """import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve;

fn handle(r: Request) -> Response {
    if r.path == "/count" {
        return new Response { status: 200, headers: new list[Header], body: str(len(split(r.body, ","))) + "\\n" };
    }
    if r.path == "/fail" {
        return new Response { status: 200, headers: new list[Header], body: slice("ab", 5, 1) };
    }
    return new Response { status: 200, headers: new list[Header], body: "ok " + str(len(r.body)) + "\\n" };
}

fn main() -> result[void] uses console, serve("PORT") {
    try serve("PORT", handle);
    print("stopped");
    return ok();
}
"""
CLIENT = """import std.http.get;

fn main() -> result[void] uses console, net("127.0.0.1") {
    for path in [PATHS] {
        match get("http://127.0.0.1:PORT" + path) {
            case ok(r) {
                print(path + ": ok " + str(r.status) + " " + str(len(r.body)));
            }
            case err(e) {
                print(path + ": err " + e);
            }
        }
    }
    return ok();
}
"""
D = tempfile.mkdtemp()
failures = []


def check(ok, what):
    print(("ok   " if ok else "FAIL ") + what, flush=True)
    if not ok:
        failures.append(what)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def start(*flags):
    port = free_port()
    path = os.path.join(D, "server_%d.vo" % port)
    open(path, "w").write(SERVER.replace("PORT", str(port)))
    err = open(path + ".err", "w+")
    p = subprocess.Popen([VO, "run", *flags, path], stdout=subprocess.PIPE, stderr=err, cwd=D)
    for _ in range(200):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
            break
        except OSError:
            time.sleep(0.05)
    return p, port, err


def send(port, data, timeout=30):
    s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    try:
        s.sendall(data)
    except OSError:
        pass
    out = b""
    try:
        while True:
            b = s.recv(65536)
            if not b:
                break
            out += b
    except OSError:
        pass
    s.close()
    return out


def status(resp):
    return resp.split(b"\r\n", 1)[0].decode(errors="replace")


def alive(p, port):
    return p.poll() is None and status(send(port, b"GET /ping HTTP/1.1\r\n\r\n", 5)) == "HTTP/1.1 200 OK"


def rss_mib(p):
    for line in open("/proc/%d/status" % p.pid):
        if line.startswith("VmRSS"):
            return int(line.split()[1]) >> 10
    return -1


def stop(p, err):
    p.send_signal(signal.SIGTERM)
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
    err.seek(0)
    return err.read()


p, port, err = start("--max-memory", "64", "--json")

# H8: many tiny chunks are decoded in C in linear time; their framing counts against the 64 KiB head
# limit, so a flood of them is a quick 431.
for n, want in ((10000, "HTTP/1.1 200 OK"), (100000, "HTTP/1.1 431 Request Header Fields Too Large")):
    t = time.time()
    r = send(port, b"POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n" + b"1\r\nx\r\n" * n + b"0\r\n\r\n")
    took = time.time() - t
    check(status(r) == want and (n > 10000 or r.endswith(b"ok %d\n" % n)) and took < 5,
          "H8: %d chunks: %s in %.2f s" % (n, status(r), took))


# H9: an extension (then a trailer) that never ends: refused at the head limit, memory stays bounded.
def flood(prefix):
    s = socket.create_connection(("127.0.0.1", port), timeout=10)
    s.sendall(b"POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n" + prefix)
    junk, sent, t0, got = b"a" * (1 << 16), 0, time.time(), b""
    try:
        while time.time() - t0 < 5:
            s.sendall(junk)
            sent += len(junk)
    except OSError:
        pass
    try:
        got = s.recv(100)
    except OSError:
        pass
    s.close()
    return sent, got


for name, prefix in (("extension", b"1;"), ("trailer", b"1\r\nx\r\n0\r\nX-T: ")):
    sent, got = flood(prefix)
    check(sent < (8 << 20) and rss_mib(p) < 150 and alive(p, port),
          "H9: endless chunk %s cut after %d KiB (%s), RSS %d MiB" % (name, sent >> 10, status(got), rss_mib(p)))

# H9: the read deadline holds against a slow drip, and a stop signal is seen during one.
s = socket.create_connection(("127.0.0.1", port), timeout=20)
s.sendall(b"POST / HTTP/1.1\r\n")
t0, closed = time.time(), False
while time.time() - t0 < 14:
    try:
        s.sendall(b"X: 1\r\n")
        s.settimeout(0.5)
        if s.recv(100) is not None:
            closed = True
            break
    except socket.timeout:
        pass
    except OSError:
        closed = True
        break
s.close()
check(closed and time.time() - t0 < 13, "H9: a slow drip is cut at the 10 s deadline (%.1f s)" % (time.time() - t0))

# H10: a handler failing with R0006 leaves the collector on: later requests do not grow memory.
body = b"," * (16 << 20)
r = send(port, b"POST /count HTTP/1.1\r\nContent-Length: %d\r\n\r\n" % len(body) + body, 60)
check(status(r) == "HTTP/1.1 500 Internal Server Error", "H10: R0006 in a handler is a 500 (%s)" % status(r))
before = rss_mib(p)
bad = [i for i in range(200) if status(send(port, b"GET /count HTTP/1.1\r\nContent-Length: 1\r\n\r\n,")) != "HTTP/1.1 200 OK"]
check(not bad and rss_mib(p) - before < 30, "H10: 200 requests after it, RSS %d -> %d MiB" % (before, rss_mib(p)))

# L11: a later error report carries no extras from the R0006 one.
check(status(send(port, b"GET /fail HTTP/1.1\r\n\r\n")) == "HTTP/1.1 500 Internal Server Error", "L11: /fail is a 500")

# H9: SIGTERM is seen while a request is being received.
s = socket.create_connection(("127.0.0.1", port))
s.sendall(b"POST / HTTP/1.1\r\nContent-Length: 100\r\n\r\nabc")
time.sleep(0.3)
t0 = time.time()
log = stop(p, err)
s.close()
check(p.returncode == 0 and time.time() - t0 < 3, "H9: SIGTERM mid-request stops the server (exit %d)" % p.returncode)
reports = [line for line in log.splitlines() if '"R00' in line]
check(len(reports) == 2 and '"in_use"' in reports[0] and '"in_use"' not in reports[1],
      "L11: the R0007 report after R0006 has no in_use (%d reports)" % len(reports))

# M2: a hostile HTTP server: every response is ok or err, the client never fails.
H = b"HTTP/1.1 200 OK\r\n"
RESP = {
    b"/cont-body": H + b"\r\n\x80",
    b"/cont-header": H + b"X:\xa9\r\n\r\nok",
    b"/cl-midchar": H + b"Content-Length: 1\r\n\r\n\xc3\xa9",
    b"/chunk-overflow": H + b"Transfer-Encoding: chunked\r\n\r\n" + b"f" * 17 + b"\r\n",
    b"/chunk-past-end": H + b"Transfer-Encoding: chunked\r\n\r\n1\r\nx",
    b"/chunk-midchar": H + b"Transfer-Encoding: chunked\r\n\r\n1\r\n\xc3\xa9\r\n0\r\n\r\n",
    b"/no-head-end": H + b"X: 1\r\n",
    b"/fine": H + b"Content-Length: 2\r\n\r\nok",
}
ls = socket.socket()
ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(("127.0.0.1", 0))
ls.listen(8)


def evil():
    while True:
        c, _ = ls.accept()
        req = b""
        while b"\r\n\r\n" not in req:
            b = c.recv(4096)
            if not b:
                break
            req += b
        path = req.split(b" ")[1] if b" " in req else b"/"
        c.sendall(RESP.get(path, H + b"Content-Length: 0\r\n\r\n"))
        c.close()


threading.Thread(target=evil, daemon=True).start()
path = os.path.join(D, "client.vo")
open(path, "w").write(CLIENT.replace("PORT", str(ls.getsockname()[1]))
                      .replace("PATHS", ", ".join('"%s"' % k.decode() for k in RESP)))
r = subprocess.run([VO, "run", path], capture_output=True, text=True, errors="replace", timeout=60)
lines = r.stdout.splitlines()
check(r.returncode == 0 and len(lines) == len(RESP) and lines[-1] == "/fine: ok 200 2",
      "M2: the client answers every hostile response (exit %d)\n    %s" % (r.returncode, "\n    ".join(lines + r.stderr.splitlines()[:3])))

print("server attacks: %d failed" % len(failures))
sys.exit(1 if failures else 0)
