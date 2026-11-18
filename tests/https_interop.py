#!/usr/bin/env python3
"""Spec 019 SC-002 (research R11): vo's HTTPS against two independent TLS implementations.

  https_interop.py [--require-tlslite]

OpenSSL: TLS 1.3 with each of its three suites (`openssl s_server -www`), and TLS 1.2 with each
accepted ECDHE-ECDSA and ECDHE-RSA suite (tests/https_server.py, Python's ssl). tlslite-ng, a pure
Python implementation (make tls-deps): TLS 1.3 and TLS 1.2. Every handshake must succeed and return a
200 response. Without tlslite-ng its checks are reported as skipped, unless --require-tlslite (make gate).
"""
import os
import socket
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))
CERTS = os.path.join(ROOT, "tests", "tls", "certs")
CA = os.path.join(CERTS, "test-ca.pem")
PROGRAM = """import std.http.get;

fn main() uses console, net("127.0.0.1") {
    match get("https://127.0.0.1:" + args()[0] + "/plain") {
        case ok(r) {
            print("status " + str(r.status));
        }
        case err(e) {
            print("err " + e);
        }
    }
}
"""


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def wait_port(port):
    for _ in range(100):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
            return
        except OSError:
            time.sleep(0.05)


def check(name, server_cmd, port, d):
    srv = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
    if port is None:
        port = int(srv.stdout.readline().split()[1])
    else:
        wait_port(port)
    r = subprocess.run([VO, "run", "--timeout", "10", "--ca-file", CA, "p.vo", "--", str(port)], cwd=d,
                       capture_output=True, text=True)
    srv.kill()
    srv.wait()
    ok = r.returncode == 0 and r.stdout.startswith("status 200")
    print(f"https_interop: {name}: {'ok' if ok else 'FAILED: ' + (r.stdout + r.stderr).strip()[:160]}")
    return ok


def main():
    bad = 0
    srv = [sys.executable, os.path.join(ROOT, "tests", "https_server.py")]
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "p.vo"), "w").write(PROGRAM)
        for suite in ("TLS_AES_128_GCM_SHA256", "TLS_AES_256_GCM_SHA384", "TLS_CHACHA20_POLY1305_SHA256"):
            p = free_port()
            cmd = ["openssl", "s_server", "-quiet", "-accept", str(p), "-tls1_3", "-ciphersuites", suite, "-www",
                   "-cert", os.path.join(CERTS, "a.test.pem"), "-cert_chain", os.path.join(CERTS, "a.test.pem"),
                   "-key", os.path.join(CERTS, "a.test.key")]
            bad += not check(f"openssl TLS 1.3 {suite}", cmd, p, d)
        for cert, suites in (("a.test", ("ECDHE-RSA-AES128-GCM-SHA256", "ECDHE-RSA-AES256-GCM-SHA384",
                                          "ECDHE-RSA-CHACHA20-POLY1305")),
                             ("a.test-ecdsa", ("ECDHE-ECDSA-AES128-GCM-SHA256", "ECDHE-ECDSA-AES256-GCM-SHA384",
                                               "ECDHE-ECDSA-CHACHA20-POLY1305"))):
            for suite in suites:
                bad += not check(f"openssl TLS 1.2 {suite}", srv + ["--cert", cert, "--tls12-only", "--cipher", suite],
                                 None, d)
        try:
            sys.path.insert(0, os.path.join(ROOT, "build", "py"))
            import tlslite  # noqa: F401
            have = True
        except ImportError:
            have = False
        if have:
            bad += not check("tlslite-ng TLS 1.3", srv + ["--cert", "a.test", "--impl", "tlslite", "--tls13-only"], None, d)
            bad += not check("tlslite-ng TLS 1.2", srv + ["--cert", "a.test", "--impl", "tlslite", "--tls12-only"], None, d)
        else:
            print("https_interop: tlslite-ng: skipped (run make tls-deps)")
            bad += "--require-tlslite" in sys.argv
    print(f"https_interop: {bad} failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
