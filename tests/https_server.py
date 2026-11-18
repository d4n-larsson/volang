#!/usr/bin/env python3
"""The HTTPS test server of spec 019 (research R11): the routes of tests/http_server.py over TLS.

  https_server.py --cert NAME [--port P] [--impl openssl|tlslite] [--tls12-only] [--tls13-only]
                  [--tls10-only] [--cipher SUITE] [--tamper] [--log FILE]

NAME is a certificate of tests/tls/certs/ (NAME.pem is the chain sent, NAME.key its key). Prints
"port N" when listening. --impl tlslite uses tlslite-ng from build/py/ (make tls-deps), a TLS
implementation independent of OpenSSL. --tls10-only offers TLS 1.0 alone (which vo refuses). --tamper
serves TLS 1.2 through a relay that flips one byte of the server's first application-data record.
--log FILE appends "request METHOD TARGET" for every request that reached the server, so a case can
show that nothing was sent before verification failed.
"""
import argparse
import os
import socket
import ssl
import sys
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "build", "py"))
import http_server  # noqa: E402

CERTS = os.path.join(HERE, "tls", "certs")


def logger(path):
    lock = threading.Lock()

    def log(method, target):
        if path:
            with lock, open(path, "a") as f:
                f.write(f"request {method} {target}\n")
    return log


def handle(conn, log):
    try:
        req = http_server.read_request(conn)
        if req:
            log(req[0], req[1])
            conn.sendall(http_server.route(*req))
    except (OSError, ValueError, ssl.SSLError):
        pass
    except Exception:  # tlslite raises its own errors
        pass
    finally:
        try:
            conn.close()
        except Exception:
            pass


def openssl_context(a):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(CERTS, a.cert + ".pem"), os.path.join(CERTS, a.cert + ".key"))
    if a.tls10_only:
        ctx.set_ciphers("DEFAULT:@SECLEVEL=0")
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1
    elif a.tls12_only or a.tamper:
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    elif a.tls13_only:
        ctx.minimum_version = ssl.TLSVersion.TLSv1_3
    if a.cipher:
        ctx.set_ciphers(a.cipher)
    return ctx


def tlslite_wrap(a):
    from tlslite import HandshakeSettings, TLSConnection, X509CertChain, parsePEMKey
    chain = X509CertChain()
    chain.parsePemList(open(os.path.join(CERTS, a.cert + ".pem")).read())
    key = parsePEMKey(open(os.path.join(CERTS, a.cert + ".key")).read(), private=True)
    settings = HandshakeSettings()
    if a.tls12_only:
        settings.minVersion = settings.maxVersion = (3, 3)
    if a.tls13_only:
        settings.minVersion = (3, 4)

    def wrap(sock):
        c = TLSConnection(sock)
        # alpn: tlslite-ng 0.8.2's TLS 1.3 server fails on a client's ALPN unless it has a list of its own
        c.handshakeServer(certChain=chain, privateKey=key, settings=settings, alpn=[bytearray(b"http/1.1")])
        return c
    return wrap


def tamper_relay(client, backend_port):
    """Relays client <-> backend; flips a byte inside the backend's first application-data record."""
    back = socket.create_connection(("127.0.0.1", backend_port))

    def up():
        try:
            while True:
                d = client.recv(65536)
                if not d:
                    break
                back.sendall(d)
        except OSError:
            pass

    threading.Thread(target=up, daemon=True).start()
    buf, flipped = b"", False
    try:
        while True:
            d = back.recv(65536)
            if not d:
                break
            buf += d
            out = b""
            while len(buf) >= 5 and len(buf) >= 5 + int.from_bytes(buf[3:5], "big"):
                n = 5 + int.from_bytes(buf[3:5], "big")
                rec, buf = bytearray(buf[:n]), buf[n:]
                if rec[0] == 23 and not flipped:
                    rec[n // 2] ^= 0x40
                    flipped = True
                out += bytes(rec)
            client.sendall(out)
    except OSError:
        pass
    finally:
        client.close()
        back.close()


def listen(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", port))
    s.listen(64)
    return s


def accept_loop(s, wrap, log):
    while True:
        conn, _ = s.accept()

        def run(c=conn):
            try:
                handle(wrap(c), log)
            except Exception:
                try:
                    c.close()
                except OSError:
                    pass
        threading.Thread(target=run, daemon=True).start()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cert", required=True)
    p.add_argument("--port", type=int, default=0)
    p.add_argument("--impl", default="openssl")
    p.add_argument("--tls12-only", action="store_true")
    p.add_argument("--tls13-only", action="store_true")
    p.add_argument("--tls10-only", action="store_true")
    p.add_argument("--cipher")
    p.add_argument("--tamper", action="store_true")
    p.add_argument("--log")
    a = p.parse_args()
    log = logger(a.log)
    if a.impl == "tlslite":
        wrap = tlslite_wrap(a)
    else:
        ctx = openssl_context(a)
        wrap = lambda c: ctx.wrap_socket(c, server_side=True)  # noqa: E731
    if a.tamper:
        backend = listen(0)
        threading.Thread(target=accept_loop, args=(backend, wrap, log), daemon=True).start()
        front = listen(a.port)
        print(f"port {front.getsockname()[1]}", flush=True)
        while True:
            c, _ = front.accept()
            threading.Thread(target=tamper_relay, args=(c, backend.getsockname()[1]), daemon=True).start()
    s = listen(a.port)
    print(f"port {s.getsockname()[1]}", flush=True)
    accept_loop(s, wrap, log)


if __name__ == "__main__":
    main()
