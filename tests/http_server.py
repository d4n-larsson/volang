#!/usr/bin/env python3
"""The HTTP test server of spec 007 (research R12, tasks T028/T043): fixed routes on 127.0.0.1.

  http_server.py [PORT]     PORT 0 (the default) picks a free one; prints "port N" when listening

Responses are written as raw bytes, so malformed and chunked replies are exact. One request per
connection; the server closes after each reply (std.http sends Connection: close).

Routes:
  GET  /plain           200, Content-Length, "hello from vo\\n"
  GET  /json            200, application/json
  GET  /chunked         200, Transfer-Encoding: chunked, three chunks with an extension
  GET  /close           200, no length: the body ends when the connection closes
  GET  /status/404      404 with a short body;  /status/500 likewise
  GET  /redirect        302 with Location: /plain (not followed by std.http)
  GET  /headers         repeated and mixed-case headers, in this order
  GET  /slow            waits 30 s before answering (for --timeout)
  GET  /delay/MS        200 "delay MS\n" after MS milliseconds (0..60000, else 400; spec 016); with
                        VO_TEST_JITTER_MS=N set, a random 0..N ms more
  GET  /peak            200, the most /delay requests in flight at once since the last /peak, then resets
  GET  /large           a body of 17 MiB (over std.http's 16 MiB limit)
  GET  /malformed       a reply with no status line
  GET  /short           Content-Length larger than the body sent
  POST /echo            200, the request's Content-Type and body back
  anything else         404
"""
import os
import random
import socket
import sys
import threading
import time

CRLF = b"\r\n"
JITTER_MS = int(os.environ.get("VO_TEST_JITTER_MS", "0"))
inflight_lock = threading.Lock()
inflight = [0, 0]  # in flight now, most at once since the last /peak


def reply(status, headers, body):
    head = [f"HTTP/1.1 {status}".encode()] + [f"{k}: {v}".encode() for k, v in headers]
    return CRLF.join(head) + CRLF + CRLF + body


def route(method, target, headers, body):
    if method == "POST" and target == "/echo":
        ctype = headers.get("content-type", "")
        out = b"content-type: " + ctype.encode() + b"\n" + body
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(out))], out)
    if method != "GET":
        return reply("405 Method Not Allowed", [("Content-Length", 0)], b"")
    if target == "/plain":
        b = b"hello from vo\n"
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(b))], b)
    if target == "/json":
        b = b'{"name": "vo", "tags": ["small", "typed"], "version": 7}'
        return reply("200 OK", [("Content-Type", "application/json"), ("Content-Length", len(b))], b)
    if target == "/chunked":
        b = b"5;ext=1\r\nhello\r\n7\r\n, chunk\r\nA\r\ned world!\n\r\n0\r\n\r\n"
        return reply("200 OK", [("Content-Type", "text/plain"), ("Transfer-Encoding", "chunked")], b)
    if target == "/close":
        return reply("200 OK", [("Content-Type", "text/plain")], b"until the connection closes\n")
    if target.startswith("/status/"):
        code = target[len("/status/"):]
        b = f"status {code}\n".encode()
        return reply(f"{code} Status", [("Content-Length", len(b))], b)
    if target == "/redirect":
        return reply("302 Found", [("Location", "/plain"), ("Content-Length", 0)], b"")
    if target == "/headers":
        return reply("200 OK", [("X-Multi", "first"), ("x-multi", "second"), ("Content-Type", "text/plain"),
                                ("X-Spaces", "  padded  "), ("Content-Length", 2)], b"ok")
    if target == "/slow":
        time.sleep(30)
        return reply("200 OK", [("Content-Length", 4)], b"late")
    if target.startswith("/delay/"):
        ms = target[len("/delay/"):]
        if not ms.isdigit() or int(ms) > 60000:
            return reply("400 Bad Request", [("Content-Length", 0)], b"")
        with inflight_lock:
            inflight[0] += 1
            inflight[1] = max(inflight[1], inflight[0])
        time.sleep((int(ms) + (random.randint(0, JITTER_MS) if JITTER_MS else 0)) / 1000)
        with inflight_lock:
            inflight[0] -= 1
        b = f"delay {int(ms)}\n".encode()
        return reply("200 OK", [("Content-Length", len(b))], b)
    if target == "/peak":
        with inflight_lock:
            b = f"{inflight[1]}\n".encode()
            inflight[1] = inflight[0]
        return reply("200 OK", [("Content-Length", len(b))], b)
    if target == "/large":
        n = 17 * 1024 * 1024
        return reply("200 OK", [("Content-Length", n)], b"x" * n)
    if target == "/malformed":
        return b"this is not HTTP\r\n\r\n"
    if target == "/short":
        return reply("200 OK", [("Content-Length", 100)], b"only a little")
    return reply("404 Not Found", [("Content-Length", 10)], b"not found\n")


def read_request(conn):
    data = b""
    while CRLF + CRLF not in data:
        chunk = conn.recv(65536)
        if not chunk:
            return None
        data += chunk
    head, _, body = data.partition(CRLF + CRLF)
    lines = head.decode("latin-1").split("\r\n")
    method, target = lines[0].split(" ")[:2]
    headers = {}
    for line in lines[1:]:
        k, _, v = line.partition(":")
        headers[k.strip().lower()] = v.strip()
    need = int(headers.get("content-length", "0"))
    while len(body) < need:
        chunk = conn.recv(65536)
        if not chunk:
            break
        body += chunk
    return method, target, headers, body[:need]  # exactly Content-Length bytes


def serve(conn):
    try:
        req = read_request(conn)
        if req:
            conn.sendall(route(*req))
    except (OSError, ValueError):
        pass
    finally:
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        conn.close()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", port))
    s.listen(128)
    print(f"port {s.getsockname()[1]}", flush=True)
    while True:
        conn, _ = s.accept()
        threading.Thread(target=serve, args=(conn,), daemon=True).start()


if __name__ == "__main__":
    main()
