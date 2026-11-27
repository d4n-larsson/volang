#!/usr/bin/env python3
"""The HTTP test server of spec 007 (research R12, tasks T028/T043): fixed routes on 127.0.0.1.

  http_server.py [PORT]     PORT 0 (the default) picks a free one; prints "port N" when listening

Responses are written as raw bytes, so malformed and chunked replies are exact. One request per
connection; the server closes after each reply (std.http sends Connection: close).

Routes:
  GET  /plain           200, Content-Length, "hello from vo\\n"
  GET  /cookie          200 with Set-Cookie and X-Api-Key headers (spec 054: redaction)
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
  GET  /big             a body of 10 MiB (spec 019: HTTPS against HTTP speed)
  GET  /malformed       a reply with no status line
  GET  /short           Content-Length larger than the body sent
  POST /echo            200, the request's Content-Type and body back
  ANY  /inspect         200, the method and target, the request's headers sorted by name, a blank line, the body
  GET  /allbytes        200, application/octet-stream, the bytes 0..255 (spec 021)
  POST /echo-bytes      200, application/octet-stream, the request's body back unchanged (spec 021)
  GET  /hex/HEX         200, text/plain, the bytes HEX stands for, whatever they are (spec 021)
  GET  /not-modified    304 with Content-Length: 1234 and no body
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


def archive(path):
    """Spec 041: package archives for tests/package_fetch.py and the packages step cases, from the
    directory $VO_ARCHIVES: /archive/NAME, /moved/NAME (one redirect), /hops/N/NAME (N + 1 redirects),
    /insecure/NAME (a redirect to http://)."""
    parts = path.strip("/").split("/")
    if parts[0] == "moved":
        return reply("302 Found", [("Location", "/archive/" + parts[1]), ("Content-Length", 0)], b"")
    if parts[0] == "hops":
        to = f"/hops/{int(parts[1]) - 1}/{parts[2]}" if int(parts[1]) > 0 else "/archive/" + parts[2]
        return reply("302 Found", [("Location", to), ("Content-Length", 0)], b"")
    if parts[0] == "insecure":
        return reply("302 Found", [("Location", "http://127.0.0.1/archive/" + parts[1]), ("Content-Length", 0)], b"")
    name = os.path.join(os.environ.get("VO_ARCHIVES", ""), os.path.basename(parts[1]))
    if not os.environ.get("VO_ARCHIVES") or not os.path.isfile(name):
        return reply("404 Not Found", [("Content-Length", 9)], b"not found")
    data = open(name, "rb").read()
    return reply("200 OK", [("Content-Type", "application/gzip"), ("Content-Length", len(data))], data)


def route(method, target, headers, body):
    if method == "GET" and target.split("?")[0].startswith(("/archive/", "/moved/", "/hops/", "/insecure/")):  # spec 041
        return archive(target.split("?")[0])
    if method == "POST" and target == "/echo-bytes":  # spec 021
        return reply("200 OK", [("Content-Type", "application/octet-stream"), ("Content-Length", len(body))], body)
    if method == "GET" and target.startswith("/hex/"):  # spec 021: any bytes, even not UTF-8
        b = bytes.fromhex(target[5:])
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(b))], b)
    if method == "GET" and target == "/allbytes":  # spec 021
        b = bytes(range(256))
        return reply("200 OK", [("Content-Type", "application/octet-stream"), ("Content-Length", len(b))], b)
    if method == "POST" and target == "/echo":
        ctype = headers.get("content-type", "")
        out = b"content-type: " + ctype.encode() + b"\n" + body
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(out))], out)
    if target == "/inspect":  # any method: the request as the server saw it
        lines = [f"{method} {target}"] + [f"{k}: {v}" for k, v in sorted(headers.items())]
        out = "\n".join(lines).encode() + b"\n\n" + body
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(out))], out)
    if method != "GET":
        return reply("405 Method Not Allowed", [("Content-Length", 0)], b"")
    if target == "/plain":
        b = b"hello from vo\n"
        return reply("200 OK", [("Content-Type", "text/plain"), ("Content-Length", len(b))], b)
    if target.split("?")[0] == "/cookie":  # spec 054: a session cookie and an API key header in the answer
        b = b"signed in\n"
        return reply("200 OK", [("Set-Cookie", "s=c-000; HttpOnly"), ("X-Api-Key", "k-answer"), ("Content-Length", len(b))], b)
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
    if target == "/big":  # spec 019 (tests/https_scale.py): 10 MiB, under the limit
        n = 10 * 1024 * 1024
        return reply("200 OK", [("Content-Length", n)], b"x" * n)
    if target == "/malformed":
        return b"this is not HTTP\r\n\r\n"
    if target == "/not-modified":  # a 304 with the Content-Length of what it stands for, and no body
        return reply("304 Not Modified", [("Content-Length", 1234)], b"")
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
