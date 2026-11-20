#!/usr/bin/env python3
"""Acceptance suite for bench/web (spec 029 SC-004): starts `vo run main.vo` (port 8765) in DIR (a fresh
data/ directory) and checks the inventory API of task.md. Prints one line per check and a summary;
exit 0 when every check passes.

  accept.py DIR
"""
import json
import os
import shutil
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VO = os.path.join(ROOT, "bin", "vo")


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def call(port, method, path, body=None):
    data = b"" if body is None else (body if isinstance(body, bytes) else json.dumps(body).encode())
    head = f"{method} {path} HTTP/1.1\r\nHost: localhost\r\n"
    if body is not None:
        head += f"Content-Type: application/json\r\nContent-Length: {len(data)}\r\n"
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=10) as s:
            s.sendall(head.encode() + b"\r\n" + data)
            out = b""
            while True:
                b = s.recv(65536)
                if not b:
                    break
                out += b
    except OSError:
        return 0, {}, None
    h, _, rest = out.partition(b"\r\n\r\n")
    lines = h.decode(errors="replace").split("\r\n")
    try:
        status = int(lines[0][9:12])
    except ValueError:
        return 0, {}, None
    headers = {k.strip().lower(): v.strip() for k, _, v in (l.partition(":") for l in lines[1:])}
    try:
        parsed = json.loads(rest) if rest else None
    except ValueError:
        parsed = "<not json>"
    return status, headers, parsed


def main():
    d = os.path.abspath(sys.argv[1])
    shutil.rmtree(os.path.join(d, "data"), ignore_errors=True)
    os.makedirs(os.path.join(d, "data"))
    port = 8765
    log = open(os.path.join(d, "server.log"), "w")
    proc = subprocess.Popen([VO, "run", "main.vo"], cwd=d, stdout=log, stderr=log)
    results = []

    def check(name, ok):
        results.append((name, bool(ok)))

    try:
        up = False
        for _ in range(100):
            if proc.poll() is not None:
                break
            try:
                socket.create_connection(("127.0.0.1", port), timeout=1).close()
                up = True
                break
            except OSError:
                time.sleep(0.05)
        check("the server starts", up)
        if up:
            run_checks(port, check)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(10)
            except subprocess.TimeoutExpired:
                proc.kill()
    for name, ok in results:
        print(("PASS " if ok else "FAIL ") + name)
    passed = sum(ok for _, ok in results)
    print(f"accept: {passed}/{len(results)}")
    return 0 if passed == len(results) and len(results) > 1 else 1


def run_checks(port, check):
    a = {"sku": "ABC123", "name": "Cup", "quantity": 5}
    b = {"sku": "XYZ9", "name": "Pot", "quantity": 50}
    s, h, j = call(port, "GET", "/items")
    check("GET /items starts empty", s == 200 and j == [])
    s, h, j = call(port, "POST", "/items", a)
    check("POST /items creates 201 with the item", s == 201 and j == dict(a, id=1))
    check("POST /items sets Location", h.get("location") == "/items/1")
    check("POST /items answers JSON", "json" in h.get("content-type", ""))
    s, h, j = call(port, "POST", "/items", b)
    check("a second item gets id 2", s == 201 and isinstance(j, dict) and j.get("id") == 2)
    s, h, j = call(port, "GET", "/items/1")
    check("GET /items/1", s == 200 and j == dict(a, id=1))
    s, h, j = call(port, "GET", "/items")
    check("GET /items lists both by id", s == 200 and j == [dict(a, id=1), dict(b, id=2)])
    s, h, j = call(port, "GET", "/items?min_quantity=10")
    check("min_quantity filters", s == 200 and j == [dict(b, id=2)])
    s, h, j = call(port, "PUT", "/items/1", {"sku": "ABC123", "name": "Mug", "quantity": 7})
    check("PUT changes the item", s == 200 and j == {"id": 1, "sku": "ABC123", "name": "Mug", "quantity": 7})
    for name, body in [("a short sku", {"sku": "AB", "name": "x", "quantity": 1}),
                       ("a lower-case sku", {"sku": "abc123", "name": "x", "quantity": 1}),
                       ("an empty name", {"sku": "QQQ1", "name": "", "quantity": 1}),
                       ("a quantity over 10000", {"sku": "QQQ1", "name": "x", "quantity": 10001}),
                       ("a negative quantity", {"sku": "QQQ1", "name": "x", "quantity": -1}),
                       ("a missing key", {"sku": "QQQ1", "name": "x"}),
                       ("a wrong type", {"sku": "QQQ1", "name": "x", "quantity": "1"}),
                       ("an extra key", {"sku": "QQQ1", "name": "x", "quantity": 1, "color": "red"})]:
        s, h, j = call(port, "POST", "/items", body)
        check(f"POST with {name} is 400 or 422", s in (400, 422) and isinstance(j, dict))
    s, h, j = call(port, "POST", "/items", b"{not json")
    check("a body that is not JSON is 400 or 422", s in (400, 422))
    s, h, j = call(port, "GET", "/items")
    check("refused requests changed nothing", s == 200 and isinstance(j, list) and len(j) == 2)
    s, h, j = call(port, "POST", "/items", {"sku": "XYZ9", "name": "Dup", "quantity": 1})
    check("a duplicate sku on POST is 409", s == 409)
    s, h, j = call(port, "PUT", "/items/1", {"sku": "XYZ9", "name": "Mug", "quantity": 7})
    check("a duplicate sku on PUT is 409", s == 409)
    s, h, j = call(port, "GET", "/items/abc")
    check("a non-numeric id is 400", s == 400)
    s, h, j = call(port, "GET", "/items/99")
    check("a missing item is 404", s == 404)
    s, h, j = call(port, "PUT", "/items/99", {"sku": "NEW1", "name": "x", "quantity": 1})
    check("PUT on a missing item is 404", s == 404)
    s, h, j = call(port, "PATCH", "/items/1", {"name": "x"})
    check("an unsupported method is 405", s == 405)
    s, h, j = call(port, "GET", "/stock")
    check("an unknown path is 404", s == 404)
    s, h, j = call(port, "DELETE", "/items/2")
    check("DELETE is 204", s == 204)
    s, h, j = call(port, "GET", "/items/2")
    check("a deleted item is gone", s == 404)


if __name__ == "__main__":
    sys.exit(main())
