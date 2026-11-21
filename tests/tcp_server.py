#!/usr/bin/env python3
"""Local peers for tests/tcp.py (spec 039, research R10).

  tcp_server.py MODE [--cert NAME]

Listens on a free loopback port and prints "port N". Modes:
  echo       every received piece is sent back
  redis      a subset of the Redis protocol: PING, SET key value, GET key (RESP requests and replies)
  silent     accepts and never sends
  greet      sends "hello\\n" and waits
  half       sends "par" (no line end) and closes
  reset      reads one piece, then resets the connection
  burst      sends 17 MiB of "x" with no line end, then "\\n"
  tls        echo over TLS from the first byte (--cert NAME of tests/tls/certs/)
  starttls   sends "220 ready\\n", waits for "STARTTLS\\n", sends "220 go\\n", then echo over TLS
"""
import argparse
import os
import socket
import ssl
import struct
import sys
import threading

CERTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tls", "certs")


def context(cert):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(CERTS, cert + ".pem"), os.path.join(CERTS, cert + ".key"))
    return ctx


def echo(c):
    while True:
        data = c.recv(65536)
        if not data:
            return
        c.sendall(data)


def read_line(c, buf):
    while b"\r\n" not in buf[0]:
        data = c.recv(65536)
        if not data:
            return None
        buf[0] += data
    line, buf[0] = buf[0].split(b"\r\n", 1)
    return line


def read_n(c, buf, n):
    while len(buf[0]) < n:
        data = c.recv(65536)
        if not data:
            return None
        buf[0] += data
    out, buf[0] = buf[0][:n], buf[0][n:]
    return out


def redis(c, store):
    buf = [b""]
    while True:
        head = read_line(c, buf)
        if head is None:
            return
        if not head.startswith(b"*"):
            c.sendall(b"-ERR expected an array\r\n")
            continue
        parts = []
        for _ in range(int(head[1:])):
            size = read_line(c, buf)
            parts.append(read_n(c, buf, int(size[1:]) + 2)[:-2])
        cmd = parts[0].upper()
        if cmd == b"PING":
            c.sendall(b"+PONG\r\n")
        elif cmd == b"SET" and len(parts) == 3:
            store[parts[1]] = parts[2]
            c.sendall(b"+OK\r\n")
        elif cmd == b"GET" and len(parts) == 2:
            v = store.get(parts[1])
            c.sendall(b"$-1\r\n" if v is None else b"$%d\r\n%s\r\n" % (len(v), v))
        else:
            c.sendall(b"-ERR unknown command\r\n")


def serve(c, a, store):
    try:
        if a.mode == "echo":
            echo(c)
        elif a.mode == "redis":
            redis(c, store)
        elif a.mode == "silent":
            while c.recv(65536):
                pass
        elif a.mode == "greet":
            c.sendall(b"hello\n")
            c.recv(1)
        elif a.mode == "half":
            c.sendall(b"par")
        elif a.mode == "reset":
            c.recv(65536)
            c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        elif a.mode == "burst":
            piece = b"x" * (1 << 20)
            for _ in range(17):
                c.sendall(piece)
            c.sendall(b"\n")
            c.recv(1)
        elif a.mode == "tls":
            echo(context(a.cert).wrap_socket(c, server_side=True))
        elif a.mode == "starttls":
            c.sendall(b"220 ready\n")
            line = b""
            while not line.endswith(b"\n"):
                data = c.recv(1)
                if not data:
                    return
                line += data
            c.sendall(b"220 go\n")
            echo(context(a.cert).wrap_socket(c, server_side=True))
    except (OSError, ssl.SSLError, ValueError):
        pass
    finally:
        try:
            c.close()
        except OSError:
            pass


def main():
    p = argparse.ArgumentParser()
    p.add_argument("mode")
    p.add_argument("--cert", default="a.test")
    a = p.parse_args()
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 0))
    s.listen(64)
    print(f"port {s.getsockname()[1]}", flush=True)
    store = {}
    while True:
        c, _ = s.accept()
        threading.Thread(target=serve, args=(c, a, store), daemon=True).start()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
