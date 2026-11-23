#!/usr/bin/env python3
"""Spec 049: a scripted PostgreSQL peer for tests/postgres.py (no database behind it).

  postgres_server.py SCRIPT [--capture FILE] [--seed N] [--password P]

Prints `port N` and serves connections until it is killed. It speaks enough of protocol 3.0 for the
client to connect, sign in and run statements; every statement gets one row with one integer column
`x` holding 1, except where the script says otherwise.

Scripts:
  plain-ok              no TLS (answers N to the request), no password asked
  tls-ok                TLS with the test certificate a.test, no password asked
  tls-bad-name          TLS with the certificate other-name
  scram-ok              no TLS; SCRAM-SHA-256 with the password (--password, default "secret")
  scram-wrong-password  SCRAM where the server knows another password: 28P01
  scram-bad-signature   SCRAM where the server's final signature is forged
  asks-md5, asks-cleartext   the server asks for another sign-in method
  no-proof              the server says "ok" in the middle of SCRAM, without its signature
  stalls-connect        accepts and says nothing
  stalls-query          signs in, then never answers a statement
  closes-in-query       closes in the middle of a statement's answer
  closes-in-commit      answers everything but `commit`, where it closes
  server-error          answers every statement with an error at position 8
  garbage               answers the first statement with malformed bytes chosen by --seed

--capture FILE appends every byte the peer receives (after TLS, the decrypted bytes).
"""
import argparse
import base64
import hashlib
import hmac
import os
import random
import socket
import ssl
import struct
import sys
import threading
import time

CERTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tls", "certs")


def context(cert):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(CERTS, cert + ".pem"), os.path.join(CERTS, cert + ".key"))
    return ctx


class Closed(Exception):
    pass


class Conn:
    def __init__(self, sock, capture):
        self.sock = sock
        self.capture = capture

    def read(self, n):
        data = b""
        while len(data) < n:
            part = self.sock.recv(n - len(data))
            if not part:
                raise Closed()
            data += part
        if self.capture:
            with open(self.capture, "ab") as f:
                f.write(data)
        return data

    def send(self, data):
        self.sock.sendall(data)

    def message(self):
        kind = self.read(1)
        size = struct.unpack(">I", self.read(4))[0]
        return kind, self.read(size - 4)


def msg(kind, body=b""):
    return kind + struct.pack(">I", len(body) + 4) + body


def error(code, text, position=None):
    body = b"SERROR\0VERROR\0C" + code.encode() + b"\0M" + text.encode() + b"\0"
    if position is not None:
        body += b"P" + str(position).encode() + b"\0"
    return msg(b"E", body + b"\0")


READY = msg(b"Z", b"I")


def one_row(tag=b"SELECT 1"):
    desc = struct.pack(">H", 1) + b"x\0" + struct.pack(">IHIhiH", 0, 0, 23, 4, -1, 0)
    row = struct.pack(">H", 1) + struct.pack(">i", 1) + b"1"
    return msg(b"1") + msg(b"2") + msg(b"T", desc) + msg(b"D", row) + msg(b"C", tag + b"\0") + READY


def scram(c, a):
    """SCRAM-SHA-256, the server's side. Returns True when the client may go on."""
    c.send(msg(b"R", struct.pack(">I", 10) + b"SCRAM-SHA-256\0\0"))
    kind, body = c.message()
    end = body.index(b"\0")
    size = struct.unpack(">i", body[end + 1:end + 5])[0]
    first = body[end + 5:end + 5 + size].decode()
    bare = first[3:]  # after "n,,"
    client_nonce = bare.split("r=", 1)[1]
    nonce = client_nonce + "srvNONCE0123456789"
    salt = b"0123456789abcdef"
    rounds = 4096
    server_first = "r=%s,s=%s,i=%d" % (nonce, base64.b64encode(salt).decode(), rounds)
    c.send(msg(b"R", struct.pack(">I", 11) + server_first.encode()))
    if a.script == "no-proof":
        c.message()
        c.send(msg(b"R", struct.pack(">I", 0)))
        return True
    kind, body = c.message()
    final = body.decode()
    without_proof, proof = final.rsplit(",p=", 1)
    password = a.password if a.script != "scram-wrong-password" else a.password + "-other"
    salted = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, rounds)
    client_key = hmac.new(salted, b"Client Key", hashlib.sha256).digest()
    stored = hashlib.sha256(client_key).digest()
    auth = (bare + "," + server_first + "," + without_proof).encode()
    signature = hmac.new(stored, auth, hashlib.sha256).digest()
    want = bytes(x ^ y for x, y in zip(client_key, signature))
    if base64.b64decode(proof) != want or without_proof != "c=biws,r=" + nonce:
        c.send(error("28P01", 'password authentication failed for user "vo"'))
        return False
    server_key = hmac.new(salted, b"Server Key", hashlib.sha256).digest()
    server_signature = hmac.new(server_key, auth, hashlib.sha256).digest()
    if a.script == "scram-bad-signature":
        server_signature = bytes([server_signature[0] ^ 1]) + server_signature[1:]
    c.send(msg(b"R", struct.pack(">I", 12) + b"v=" + base64.b64encode(server_signature)))
    c.send(msg(b"R", struct.pack(">I", 0)))
    return True


def garbage(seed):
    """A malformed answer to a statement, chosen by the seed. The second value says whether to close."""
    rng = random.Random(seed)
    kind = rng.randrange(12)
    if kind == 0:
        return bytes(rng.randrange(256) for _ in range(rng.randint(1, 200))), True
    if kind == 1:
        return b"T" + struct.pack(">I", 0xFFFFFFF0), True
    if kind == 2:
        return b"D" + struct.pack(">I", rng.randrange(4)), True
    if kind == 3:
        return msg(b"1") + msg(b"2") + b"T" + struct.pack(">I", 500) + b"x\0", True
    if kind == 4:
        return msg(bytes([rng.choice(b"?!xyzQWP@")]), b"abc"), True
    if kind == 5:
        row = struct.pack(">H", 1) + struct.pack(">i", 1) + b"1"
        return msg(b"1") + msg(b"2") + msg(b"D", row) + READY, False
    if kind == 6:
        desc = struct.pack(">H", 1) + b"x\0" + struct.pack(">IHIhiH", 0, 0, 23, 4, -1, 0)
        row = struct.pack(">H", 3) + struct.pack(">i", 1) + b"1"
        return msg(b"T", desc) + msg(b"D", row) + READY, False
    if kind == 7:
        desc = struct.pack(">H", 1) + b"x\0" + struct.pack(">IHIhiH", 0, 0, 23, 4, -1, 0)
        row = struct.pack(">H", 1) + struct.pack(">i", rng.choice([-5, 100, 2 ** 31 - 1])) + b"1"
        return msg(b"T", desc) + msg(b"D", row) + READY, False
    if kind == 8:
        desc = struct.pack(">H", rng.choice([1, 7, 65535])) + b"x"
        return msg(b"T", desc) + READY, False
    if kind == 9:
        desc = struct.pack(">H", 1) + b"x\0" + struct.pack(">IHIhiH", 0, 0, 23, 4, -1, 0)
        row = struct.pack(">H", 1) + struct.pack(">i", 3) + b"abc"
        return msg(b"T", desc) + msg(b"D", row) + msg(b"C", b"SELECT 1\0") + READY, False
    if kind == 10:
        return msg(b"E", bytes(rng.randrange(1, 256) for _ in range(rng.randint(0, 40)))) + READY, False
    desc = struct.pack(">H", 1) + b"\xff\xfe\0" + struct.pack(">IHIhiH", 0, 0, 25, -1, -1, 0)
    row = struct.pack(">H", 1) + struct.pack(">i", 2) + b"\xff\xfe"
    return msg(b"T", desc) + msg(b"D", row) + msg(b"C", b"SELECT 1\0") + READY, False


def statements(c, a):
    while True:
        text = b""
        while True:
            kind, body = c.message()
            if kind == b"X":
                return
            if kind == b"P":
                text = body[1:body.index(b"\0", 1)]
            if kind == b"S":
                break
        if a.script == "stalls-query":
            time.sleep(3600)
        if a.script == "closes-in-query":
            c.send(msg(b"1") + msg(b"2"))
            return
        if a.script == "closes-in-commit" and text == b"commit":
            return
        if a.script == "server-error":
            c.send(error("42703", 'column "nope" does not exist', 8) + READY)
            continue
        if a.script == "garbage":
            data, close = garbage(a.seed)
            c.send(data)
            if close:
                return
            continue
        tag = b"SELECT 1"
        word = text.split(b" ")[0].upper()
        if word in (b"BEGIN", b"COMMIT", b"ROLLBACK"):
            c.send(msg(b"1") + msg(b"2") + msg(b"n") + msg(b"C", word + b"\0") + READY)
            continue
        c.send(one_row(tag))


def serve(sock, a):
    c = Conn(sock, a.capture)
    try:
        if a.script == "stalls-connect":
            time.sleep(3600)
        size = struct.unpack(">I", c.read(4))[0]
        body = c.read(size - 4)
        if struct.unpack(">I", body[:4])[0] == 80877103:  # the TLS request
            if a.script in ("tls-ok", "tls-bad-name"):
                sock.sendall(b"S")
                cert = "a.test" if a.script == "tls-ok" else "other-name"
                c.sock = context(cert).wrap_socket(sock, server_side=True)
            else:
                sock.sendall(b"N")
            size = struct.unpack(">I", c.read(4))[0]
            body = c.read(size - 4)
        if a.script == "asks-md5":
            c.send(msg(b"R", struct.pack(">I", 5) + b"salt"))
            c.message()
            return
        if a.script == "asks-cleartext":
            c.send(msg(b"R", struct.pack(">I", 3)))
            c.message()
            return
        if a.script.startswith("scram") or a.script == "no-proof":
            if not scram(c, a):
                return
        else:
            c.send(msg(b"R", struct.pack(">I", 0)))
        c.send(msg(b"S", b"server_version\0" + b"17.0\0") + msg(b"K", struct.pack(">II", 1, 2)) + READY)
        statements(c, a)
    except (Closed, OSError, ssl.SSLError, ValueError, struct.error):
        pass
    finally:
        try:
            c.sock.close()
        except OSError:
            pass


def main():
    p = argparse.ArgumentParser()
    p.add_argument("script")
    p.add_argument("--capture")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--password", default="secret")
    a = p.parse_args()
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 0))
    s.listen(64)
    print(f"port {s.getsockname()[1]}", flush=True)
    while True:
        c, _ = s.accept()
        threading.Thread(target=serve, args=(c, a), daemon=True).start()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
