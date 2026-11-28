#!/usr/bin/env python3
"""Spec 063: a WebSocket peer written from RFC 6455 with the Python standard library, for tests/events.py.

Server (vo is the client): Server(behaviour, handshake=...) listens on 127.0.0.1; .port. Each connection
reads the handshake, answers it as `handshake` says (status, accept value, extension, sub-protocol), then
runs the behaviour: "echo", ("script", raw_bytes) (sends the bytes, then reads frames until the peer
closes and keeps them in .seen), or a function(conn).

Client (vo is the server): client_connect(port, path, headers) does the handshake and returns a Conn.

HOSTILE: named byte sequences that break the protocol, with what the receiving side must do: the error
it reports (a word) and the close code it sends. server_frames(...) builds them unmasked (sent by a server),
client_frames(...) masked (sent by a client).
"""
import base64
import hashlib
import os
import socket
import struct
import threading
import time

GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def accept_for(key):
    return base64.b64encode(hashlib.sha1(key.encode() + GUID).digest()).decode()


def xor(data, key):
    """data XOR the 4-byte key, repeated (fast for large payloads)."""
    n = len(data)
    if not n:
        return b""
    k = (key * (n // 4 + 1))[:n]
    return (int.from_bytes(data, "big") ^ int.from_bytes(k, "big")).to_bytes(n, "big")


def frame(opcode, payload=b"", fin=True, rsv=0, mask=None, length=None):
    """One frame. mask: 4 bytes (masked) or None. length: force a length form ("16", "64", "64top")."""
    b0 = (0x80 if fin else 0) | (rsv << 4) | opcode
    n = len(payload)
    mbit = 0x80 if mask is not None else 0
    if length == "16" or (length is None and 126 <= n < 65536):
        head = bytes([b0, mbit | 126]) + struct.pack(">H", n)
    elif length in ("64", "64top") or (length is None and n >= 65536):
        v = n | (1 << 63) if length == "64top" else n
        head = bytes([b0, mbit | 127]) + struct.pack(">Q", v)
    else:
        head = bytes([b0, mbit | n])
    if mask is not None:
        payload = xor(payload, mask)
        head += mask
    return head + payload


def close_payload(code, reason=b""):
    return struct.pack(">H", code) + reason


class Conn:
    """One side of a connection after the handshake: frames in and out."""

    def __init__(self, sock, client):
        self.sock = sock
        self.client = client  # a client masks what it sends
        self.buf = b""

    def raw(self, data):
        self.sock.sendall(data)

    def send(self, opcode, payload=b"", fin=True):
        self.raw(frame(opcode, payload, fin, mask=os.urandom(4) if self.client else None))

    def _need(self, n):
        while len(self.buf) < n:
            got = self.sock.recv(65536)
            if not got:
                raise EOFError
            self.buf += got
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def read(self):
        """(fin, opcode, payload, masked) of the next frame; EOFError when the connection ends."""
        b0, b1 = self._need(2)
        n = b1 & 0x7F
        if n == 126:
            n = struct.unpack(">H", self._need(2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self._need(8))[0]
        key = self._need(4) if b1 & 0x80 else None
        payload = self._need(n)
        if key:
            payload = xor(payload, key)
        return bool(b0 & 0x80), b0 & 0x0F, payload, key is not None

    def messages(self, answer_pings=True):
        """Every frame until the connection ends: [(opcode, payload, masked)]; a close is echoed."""
        seen = []
        try:
            while True:
                fin, op, payload, masked = self.read()
                seen.append((op, payload, masked))
                if op == 9 and answer_pings:
                    self.send(10, payload)
                if op == 8:
                    try:
                        self.send(8, payload[:2])
                    except OSError:
                        pass
                    break
        except (EOFError, OSError):
            pass
        return seen


def read_head(sock):
    data = b""
    while b"\r\n\r\n" not in data:
        got = sock.recv(4096)
        if not got:
            raise EOFError
        data += got
    head, rest = data.split(b"\r\n\r\n", 1)
    lines = head.decode("latin-1").split("\r\n")
    headers = {}
    for line in lines[1:]:
        k, _, v = line.partition(":")
        headers[k.strip().lower()] = v.strip()
    return lines[0], headers, rest


class Server:
    """A WebSocket server on 127.0.0.1 (vo connects to it as a client)."""

    def __init__(self, behaviour="echo", handshake=None, tls=None):
        self.behaviour = behaviour
        self.tls = None
        if tls:  # a certificate of tests/tls/certs/
            import ssl
            certs = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tls", "certs")
            self.tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.tls.load_cert_chain(os.path.join(certs, tls + ".pem"), os.path.join(certs, tls + ".key"))
        self.events = []  # (time, what, path)
        self.handshake = handshake or {}
        self.requests = []
        self.seen = []
        self.seen_by = {}
        self.done = threading.Event()
        self.lsock = socket.socket()
        self.lsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.lsock.bind(("127.0.0.1", 0))
        self.lsock.listen(64)
        self.port = self.lsock.getsockname()[1]
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while True:
            try:
                sock, _ = self.lsock.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(sock,), daemon=True).start()

    def _serve(self, sock):
        path = "?"
        try:
            if self.tls:
                sock = self.tls.wrap_socket(sock, server_side=True)
            line, headers, rest = read_head(sock)
            self.requests.append((line, headers))
            path = line.split()[1]
            self.events.append((time.time(), "open", path))
            h = self.handshake
            status = h.get("status", 101)
            if status != 101:
                sock.sendall(f"HTTP/1.1 {status} No\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".encode())
                return
            accept = h.get("accept", accept_for(headers.get("sec-websocket-key", "")))
            out = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            out += f"Sec-WebSocket-Accept: {accept}\r\n"
            if "extension" in h:
                out += f"Sec-WebSocket-Extensions: {h['extension']}\r\n"
            protocol = h.get("protocol")
            if protocol is None and headers.get("sec-websocket-protocol"):
                protocol = headers["sec-websocket-protocol"].split(",")[0].strip()
            if protocol:
                out += f"Sec-WebSocket-Protocol: {protocol}\r\n"
            sock.sendall(out.encode() + b"\r\n")
            conn = Conn(sock, client=False)
            conn.buf = rest
            b = self.behaviour
            if b == "echo":
                self._echo(conn)
            elif isinstance(b, tuple) and b[0] == "script":
                conn.raw(b[1])
                self.seen.append(conn.messages())
            elif isinstance(b, dict):  # a script per request path, sent while what comes back is read
                path = line.split()[1]
                script = b.get(path, b"")
                fin = isinstance(script, tuple)  # ("fin", bytes): the peer then ends its side
                if fin:
                    script = script[1]
                threading.Thread(target=self._send_quietly, args=(conn, script, fin), daemon=True).start()
                self.seen_by[path] = conn.messages()
            else:
                b(conn)
        except (EOFError, OSError):
            pass
        finally:
            self.events.append((time.time(), "end", path))
            self.done.set()
            try:
                sock.close()
            except OSError:
                pass

    def _send_quietly(self, conn, data, fin=False):
        try:
            conn.raw(data)
            if fin:
                conn.sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass

    def _echo(self, conn):
        parts, kind = [], 0
        while True:
            fin, op, payload, masked = conn.read()
            if op == 8:
                conn.send(8, payload[:2])
                return
            if op == 9:
                conn.send(10, payload)
                continue
            if op in (1, 2):
                kind, parts = op, [payload]
            elif op == 0:
                parts.append(payload)
            if fin and op in (0, 1, 2):
                conn.send(kind, b"".join(parts))

    def stop(self):
        try:
            self.lsock.close()
        except OSError:
            pass


def client_connect(port, path="/", headers=None, key=None):
    """A handshake as a client: (status, response headers, Conn or None)."""
    sock = socket.create_connection(("127.0.0.1", port), timeout=30)
    key = key or base64.b64encode(os.urandom(16)).decode()
    req = f"GET {path} HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
    req += f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
    for k, v in (headers or {}).items():
        req += f"{k}: {v}\r\n"
    sock.sendall(req.encode() + b"\r\n")
    line, rh, rest = read_head(sock)
    status = int(line.split()[1])
    if status != 101:
        sock.close()
        return status, rh, None
    conn = Conn(sock, client=True)
    conn.buf = rest
    return status, rh, conn


# ---- hostile sequences ----

def bad_utf8():
    """Byte strings that are not UTF-8 (overlong forms, surrogates, beyond U+10FFFF, stray and missing
    continuation bytes), each with valid text around it."""
    seqs = [b"\x80", b"\xbf", b"\xc0\x80", b"\xc1\xbf", b"\xe0\x80\x80", b"\xe0\x9f\xbf", b"\xf0\x80\x80\x80",
            b"\xf0\x8f\xbf\xbf", b"\xed\xa0\x80", b"\xed\xbf\xbf", b"\xed\xa0\x80\xed\xb0\x80", b"\xf4\x90\x80\x80",
            b"\xf5\x80\x80\x80", b"\xf8\x88\x80\x80\x80", b"\xfc\x84\x80\x80\x80\x80", b"\xfe", b"\xff",
            b"\xc2", b"\xe2\x82", b"\xf0\x9f\x98", b"\xc2\x41", b"\xe2\x41\x82", b"\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5\xed\xa0\x80\x65\x64\x69\x74\x65\x64",
            b"\xef\xbf\xbe\xed\xa0", b"\x41\x80\x42", b"\xc0\xaf", b"\xe0\x80\xaf", b"\xf0\x80\x80\xaf", b"\xc1\x81",
            b"\xed\xae\x80", b"\xed\xaf\xbf", b"\xed\xb0\x80", b"\xed\xbe\x80", b"\xf7\xbf\xbf\xbf"]
    return [b"ok " + s + b" ok" for s in seqs]


CLOSE_CODES_BAD = [0, 999, 1004, 1005, 1006, 1015, 1016, 1100, 2000, 2999, 5000, 65535]
CLOSE_CODES_GOOD = [1000, 1001, 1002, 1003, 1007, 1008, 1009, 1010, 1011, 3000, 3999, 4000, 4999]


def hostile(masked):
    """[(name, raw bytes, error word, close code the receiver sends or None)] for frames sent by a
    client (masked) or a server (not masked)."""
    m = (lambda: os.urandom(4)) if masked else (lambda: None)
    f = lambda op, p=b"", fin=True, rsv=0, length=None: frame(op, p, fin, rsv, m(), length)  # noqa: E731
    out = []
    for rsv in range(1, 8):
        for op in (1, 2, 9):
            out.append((f"rsv {rsv} on opcode {op}", f(op, b"x", rsv=rsv), "protocol", 1002))
    for op in (3, 4, 5, 6, 7, 11, 12, 13, 14, 15):
        out.append((f"reserved opcode {op}", f(op, b"x"), "protocol", 1002))
        out.append((f"reserved opcode {op}, empty, not final", f(op, b"", fin=False), "protocol", 1002))
    wrong = frame(1, b"x", mask=None if masked else os.urandom(4))
    out.append(("wrong masking", wrong, "protocol", 1002))
    out.append(("ping of 126 bytes", f(9, b"x" * 126), "protocol", 1002))
    out.append(("fragmented ping", f(9, b"x", fin=False), "protocol", 1002))
    out.append(("close of 126 bytes", f(8, close_payload(1000, b"x" * 124)), "protocol", 1002))
    out.append(("pong of 200 bytes", f(10, b"x" * 200), "protocol", 1002))
    out.append(("continuation without a start", f(0, b"x"), "protocol", 1002))
    out.append(("continuation without a start, final", f(0, b"x", fin=True), "protocol", 1002))
    out.append(("text inside a fragmented text", f(1, b"a", fin=False) + f(1, b"b"), "protocol", 1002))
    out.append(("binary inside a fragmented text", f(1, b"a", fin=False) + f(2, b"b"), "protocol", 1002))
    out.append(("text inside a fragmented binary", f(2, b"a", fin=False) + f(1, b"b"), "protocol", 1002))
    out.append(("length 5 in 16 bits", f(2, b"abcde", length="16"), "protocol", 1002))
    out.append(("length 5 in 64 bits", f(2, b"abcde", length="64"), "protocol", 1002))
    out.append(("length 200 in 64 bits", f(2, b"x" * 200, length="64"), "protocol", 1002))
    out.append(("64-bit length with its top bit set", f(2, b"abc", length="64top"), "protocol", 1002))
    out.append(("close of one byte", f(8, b"\x03"), "protocol", 1002))
    for code in CLOSE_CODES_BAD:
        out.append((f"close code {code}", f(8, close_payload(code)), "protocol", 1002))
    out.append(("close reason not UTF-8", f(8, close_payload(1000, b"\xff\xfe")), "protocol", 1007))
    for i, s in enumerate(bad_utf8()):
        out.append((f"text not UTF-8 #{i}", f(1, s), "not_text", 1007))
        half = len(s) // 2
        out.append((f"text not UTF-8 in fragments #{i}", f(1, s[:half], fin=False) + f(0, s[half:]), "not_text", 1007))
    out.append(("message one byte over 1 MiB", f(2, b"x" * (1048576 + 1)), "too_large", 1009))
    out.append(("fragments over 1 MiB together", f(2, b"x" * 600000, fin=False) + f(0, b"x" * 600000), "too_large", 1009))
    huge = bytes([0x82, (0x80 if masked else 0) | 127]) + struct.pack(">Q", 1 << 40) + (os.urandom(4) if masked else b"")
    out.append(("length of 2^40", huge, "too_large", 1009))
    return out


def good(masked):
    """[(name, raw bytes, the messages the receiver reports)] for frames that follow the rules."""
    m = (lambda: os.urandom(4)) if masked else (lambda: None)
    f = lambda op, p=b"", fin=True: frame(op, p, fin, 0, m())  # noqa: E731
    out = [
        ("empty text", f(1, b""), ["text 0 "]),
        ("empty binary", f(2, b""), ["binary 0"]),
        ("text in three fragments", f(1, b"ab", fin=False) + f(0, b"cd", fin=False) + f(0, b"ef"), ["text 6 abcdef"]),
        ("a character split across fragments", f(1, "å".encode()[:1], fin=False) + f(0, "å".encode()[1:]), ["text 2 å"]),
        ("a ping between fragments", f(1, b"ab", fin=False) + f(9, b"p") + f(0, b"cd"), ["text 4 abcd"]),
        ("1000 fragments", f(2, b"x", fin=False) + b"".join(f(0, b"x", fin=False) for _ in range(998)) + f(0, b"x"), ["binary 1000"]),
        ("exactly 1 MiB", f(2, b"y" * 1048576), ["binary 1048576"]),
        ("empty fragments", f(1, b"", fin=False) + f(0, b"", fin=False) + f(0, b"a"), ["text 1 a"]),
    ]
    for code in CLOSE_CODES_GOOD:
        out.append((f"close code {code}", f(8, close_payload(code, b"bye")), [f"closed {code} bye"]))
    out.append(("close without a code", f(8, b""), ["closed 1005 "]))
    out.append(("close with a code and no reason", f(8, close_payload(1000)), ["closed 1000 "]))
    out.append(("close code 1012 to 1014 from a server", f(8, close_payload(1013)), ["closed 1013 "]))
    return out


def mutated(rng, seqs, n):
    """n byte sequences made from seqs by flipping, inserting, deleting and cutting bytes."""
    out = []
    for _ in range(n):
        b = bytearray(rng.choice(seqs))
        for _ in range(rng.randint(1, 4)):
            what = rng.randint(0, 3)
            if what == 0 and b:
                b[rng.randrange(len(b))] ^= 1 << rng.randrange(8)
            elif what == 1:
                b.insert(rng.randrange(len(b) + 1), rng.randrange(256))
            elif what == 2 and b:
                del b[rng.randrange(len(b))]
            elif b:
                del b[rng.randrange(len(b)):]
        out.append(bytes(b))
    return out
