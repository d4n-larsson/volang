#!/usr/bin/env python3
"""Spec 019 SC-003 (research R8): hostile TLS servers never crash, hang or exhaust vo.

  tls_fuzz.py [--runs N] [--seed S]     (defaults 1000 and 1)

Captures the first flight a real server (tests/https_server.py, OpenSSL) sends during a handshake,
then serves N mutations of it, deterministically seeded: bit flips, length fields set to 0, maximum
or random values, truncation, records split at random points, and random bytes appended. For each,
vo runs `get("https://127.0.0.1:PORT/plain")` with --timeout 5 and --ca-file: every run must end
with exit 0 (an `err` value, or a response) within the time limit, never a runtime error (70, such
as R0006), an internal error (125) or a crash.
"""
import os
import random
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))
CA = os.path.join(ROOT, "tests", "tls", "certs", "test-ca.pem")
PROGRAM = """import std.http;

fn main() uses console, net("127.0.0.1") {
    match http.get("https://127.0.0.1:" + args()[0] + "/plain") {
        case ok(r) {
            print("status " + str(r.status));
        }
        case err(e) {
            print("err " + e);
        }
    }
}
"""


def arg(name, default):
    return int(sys.argv[sys.argv.index(name) + 1]) if name in sys.argv else default


def capture_flight():
    """The bytes a real server sends after the ClientHello, up to the client's next message."""
    srv = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "https_server.py"), "--cert", "a.test"],
                           stdout=subprocess.PIPE)
    real = int(srv.stdout.readline().split()[1])
    lst = socket.socket()
    lst.bind(("127.0.0.1", 0))
    lst.listen(1)
    flight = []

    def relay():
        c, _ = lst.accept()
        b = socket.create_connection(("127.0.0.1", real))
        b.sendall(c.recv(65536))  # the ClientHello
        b.settimeout(1.0)
        data = b""
        try:
            while True:
                d = b.recv(65536)
                if not d:
                    break
                data += d
        except socket.timeout:
            pass
        flight.append(data)
        c.close()
        b.close()

    t = threading.Thread(target=relay)
    t.start()
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "f.vo"), "w").write(PROGRAM)
        subprocess.run([VO, "run", "--timeout", "5", "--ca-file", CA, "f.vo", "--", str(lst.getsockname()[1])],
                       cwd=d, capture_output=True)
    t.join()
    srv.kill()
    return flight[0]


def mutate(flight, rnd):
    data = bytearray(flight)
    kind = rnd.randrange(6)
    if kind == 0:  # bit flips
        for _ in range(rnd.randint(1, 8)):
            data[rnd.randrange(len(data))] ^= 1 << rnd.randrange(8)
    elif kind == 1:  # a record or handshake length field at 0, max or random
        i = rnd.randrange(len(data) - 2)
        data[i:i + 2] = rnd.choice([b"\x00\x00", b"\xff\xff", bytes([rnd.randrange(256), rnd.randrange(256)])])
    elif kind == 2:  # truncated
        data = data[:rnd.randrange(len(data))]
    elif kind == 3:  # random bytes appended
        data += bytes(rnd.randrange(256) for _ in range(rnd.randint(1, 5000)))
    elif kind == 4:  # a random span replaced
        i = rnd.randrange(len(data))
        n = rnd.randint(1, 64)
        data[i:i + n] = bytes(rnd.randrange(256) for _ in range(n))
    return bytes(data), kind == 5  # 5: the flight sent unchanged, split into small writes


def main():
    runs, seed = arg("--runs", 1000), arg("--seed", 1)
    flight = capture_flight()
    rnd = random.Random(seed)
    lst = socket.socket()
    lst.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    lst.bind(("127.0.0.1", 0))
    lst.listen(16)
    port = lst.getsockname()[1]
    bad, kinds = [], {}
    start = time.monotonic()
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "f.vo"), "w").write(PROGRAM)
        for n in range(runs):
            data, split = mutate(flight, rnd)

            def serve():
                c, _ = lst.accept()
                try:
                    c.recv(65536)
                    if split:
                        for i in range(0, len(data), 7):
                            c.sendall(data[i:i + 7])
                    else:
                        c.sendall(data)
                    c.settimeout(2.0)
                    c.recv(65536)
                except OSError:
                    pass
                c.close()

            t = threading.Thread(target=serve)
            t.start()
            t0 = time.monotonic()
            r = subprocess.run([VO, "run", "--timeout", "5", "--ca-file", CA, "f.vo", "--", str(port)],
                               cwd=d, capture_output=True, text=True, timeout=30)
            dt = time.monotonic() - t0
            t.join()
            first = (r.stdout.split("\n")[0] or r.stderr.split("\n")[0])[:60]
            kinds[first.split(" 127")[0]] = kinds.get(first.split(" 127")[0], 0) + 1
            if r.returncode != 0 or dt > 7:
                bad.append(f"run {n}: exit {r.returncode} in {dt:.1f} s: {(r.stdout + r.stderr)[-200:]!r}")
    for b in bad[:10]:
        print("FAIL", b)
    print(f"tls_fuzz: {runs} hostile handshakes (seed {seed}) in {time.monotonic() - start:.0f} s: "
          f"{len(bad)} crashed, hung or failed")
    for k, v in sorted(kinds.items(), key=lambda kv: -kv[1]):
        print(f"  {v:5d}  {k}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
