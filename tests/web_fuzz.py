#!/usr/bin/env python3
"""Spec 029 SC-003: malformed requests against a 10-operation std.web application.

Serves the application (serve_app, 4 workers) and sends generated requests that are wrong by
construction: unknown paths, other methods, bad path variables, bodies that are not JSON or not of the
right shape, missing and unknown fields, broken limits, bad query values, huge numbers, deep nesting,
bad percent-encoding, another content type. Every answer must be the documented 4xx problem
(application/problem+json, its "status" the response's) with the expected status, and no operation may
run (operations answer 200 with "ran"). No request may answer 500.

  web_fuzz.py [--count N] [--seed S]   (default 1,000 requests)
  web_fuzz.py --quick                  (200 requests, make test)
"""
import json
import os
import random
import socket
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serve_workers import Server  # noqa: E402

PROGRAM = """import std.http.Response;
import std.web.App;
import std.web.ErrorStatus;
import std.web.NoInput;
import std.web.Route;
import std.web.Step;
import std.web.json_reply;
import std.web.on_delete;
import std.web.on_get;
import std.web.on_patch;
import std.web.on_post;
import std.web.on_put;
import std.web.serve_app;

variant Level {
    low;
    high;
}

record Item {
    @size(1, 20)
    name: str;
    @range(0, 100)
    count: int;
    @size(0, 3)
    @default([])
    tags: list[str];
    @default(Level.low)
    level: Level;
}

record ItemId {
    id: int;
}

record NewItem {
    body: Item;
}

record Change {
    id: int;
    body: Item;
}

record Listing {
    @range(1, 50)
    @default(10)
    limit: int;
    @default(false)
    done: bool;
    @default(Level.low)
    level: Level;
}

record Ratio {
    @range(0.0, 1.0)
    value: float;
}

record Scale {
    id: int;
    body: Ratio;
}

record Slug {
    @pattern("[a-z]+")
    slug: str;
}

fn ran(n: int, input: NoInput) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_id(n: int, input: ItemId) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_new(n: int, input: NewItem) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_change(n: int, input: Change) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_list(n: int, input: Listing) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_scale(n: int, input: Scale) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn ran_slug(n: int, input: Slug) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn routes() -> list[Route[int]] {
    return [on_get("/items", ran_list), on_post("/items", ran_new), on_get("/items/{id}", ran_id), on_put("/items/{id}", ran_change), on_patch("/items/{id}/scale", ran_scale), on_delete("/items/{id}", ran_id), on_get("/tags/{slug}", ran_slug), on_get("/ping", ran), on_post("/ping", ran), on_get("/items/{id}/scale", ran_id)];
}

fn main() -> result[void] uses serve("PORT") {
    return serve_app("PORT", 4, fn() -> result[int] { return ok(0); }, new App { routes: routes(), steps: new list[Step[int]], errors: new list[ErrorStatus] });
}
"""

GOOD_ITEM = {"name": "cup", "count": 3}


def body_cases(rng):
    """(body text, expected status) for POST /items: each wrong by construction."""
    k = rng.randrange(9)
    if k == 0:  # not JSON
        return rng.choice(["{", "nope", "{\"name\": }", "[1,", "\"unterminated", "{'name': 'x'}"]), 400
    if k == 1:  # a field of the wrong JSON type
        f, v = rng.choice([("name", 3), ("count", "3"), ("tags", "a"), ("level", 1), ("count", True), ("tags", [1])])
        return json.dumps(dict(GOOD_ITEM, **{f: v})), 400
    if k == 2:  # a required field missing
        d = dict(GOOD_ITEM)
        del d[rng.choice(["name", "count"])]
        return json.dumps(d), 400
    if k == 3:  # an unknown field
        return json.dumps(dict(GOOD_ITEM, **{rng.choice(["nam", "Count", "extra", "x y"]): 1})), 400
    if k == 4:  # only limits broken (well-formed, right shape)
        d = rng.choice([{"name": "", "count": 3}, {"name": "x" * 21, "count": 3}, {"name": "a", "count": 101},
                        {"name": "a", "count": -1}, {"name": "a", "count": 1, "tags": ["a", "b", "c", "d"]}])
        return json.dumps(d), 422
    if k == 5:  # an unknown case
        return json.dumps(dict(GOOD_ITEM, level=rng.choice(["mid", "LOW", ""]))), 400
    if k == 6:  # numbers no int holds, or not whole
        return json.dumps(dict(GOOD_ITEM)).replace("3}", rng.choice(["1e30}", "3.5}", "99999999999999999999}"])), 400
    if k == 7:  # nested far too deeply
        depth = rng.randrange(600, 2000)
        return "{\"name\": \"a\", \"count\": 1, \"tags\": " + "[" * depth + "]" * depth + "}", 400
    return json.dumps([GOOD_ITEM]), 400  # the wrong top-level shape


def request_case(rng):
    """(method, path, headers, body, expected status, category)."""
    k = rng.randrange(8)
    if k == 0:
        return "GET", rng.choice(["/", "/item", "/items/1/x", "/ping/1", "/tags", "/ITEMS"]), {}, "", 404, "unknown path"
    if k == 1:
        m, p = rng.choice([("DELETE", "/items"), ("PUT", "/items"), ("POST", "/items/1"), ("PATCH", "/ping"),
                           ("DELETE", "/tags/abc"), ("POST", "/items/1/scale")])
        return m, p, {}, "", 405, "other method"
    if k == 2:
        return "GET", "/items/" + rng.choice(["x", "1.5", "99999999999999999999", "-", "1e3", "%31a"]), {}, "", 400, "path variable"
    if k == 3:
        b, s = body_cases(rng)
        return "POST", "/items", {"Content-Type": "application/json"}, b, s, "body"
    if k == 4:
        q = rng.choice(["limit=x", "limit=0", "limit=51", "done=maybe", "level=mid", "limit=1&limit=2", "limit=1.5"])
        return "GET", "/items?" + q, {}, "", 422 if q in ("limit=0", "limit=51") else 400, "query"
    if k == 5:
        return "POST", "/items", {"Content-Type": rng.choice(["text/plain", "application/x-www-form-urlencoded"])}, \
            json.dumps(GOOD_ITEM), 415, "content type"
    if k == 6:
        return "GET", "/tags/" + rng.choice(["ABC", "a1", "a-b"]), {}, "", 422, "pattern"
    return "PATCH", "/items/1/scale", {"Content-Type": "application/json"}, \
        json.dumps({"value": rng.choice([1.5, -0.1, "0.5", None])}), None, "float"


def send(port, method, path, headers, body):
    head = f"{method} {path} HTTP/1.1\r\nHost: localhost\r\n" + "".join(f"{k}: {v}\r\n" for k, v in headers.items())
    data = body.encode()
    if data:
        head += f"Content-Length: {len(data)}\r\n"
    with socket.create_connection(("127.0.0.1", port), timeout=30) as s:
        s.sendall(head.encode() + b"\r\n" + data)
        out = b""
        while True:
            b = s.recv(65536)
            if not b:
                break
            out += b
    head, _, rest = out.partition(b"\r\n\r\n")
    return int(head[9:12]), head.decode(errors="replace"), rest


def main():
    args = sys.argv[1:]
    count = 200 if "--quick" in args else int(args[args.index("--count") + 1]) if "--count" in args else 1000
    seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 1
    rng = random.Random(seed)
    problems, seen = [], {}
    with Server(PROGRAM) as srv:
        for _ in range(count):
            m, p, h, b, want, cat = request_case(rng)
            seen[cat] = seen.get(cat, 0) + 1
            status, head, body = send(srv.port, m, p, h, b)
            wrong = None
            if b'"ran"' in body:
                wrong = "the operation ran"
            elif status >= 500:
                wrong = "a server error"
            elif want is not None and status != want:
                wrong = f"status {status}, want {want}"
            elif want is None and status not in (400, 422):
                wrong = f"status {status}, want 400 or 422"
            elif "application/problem+json" not in head:
                wrong = "not a problem body"
            else:
                try:
                    if json.loads(body)["status"] != status:
                        wrong = "the problem's status differs"
                except (ValueError, KeyError):
                    wrong = "a problem body that is not JSON"
            if wrong:
                problems.append(f"{cat}: {m} {p} {b[:80]!r}: {wrong}: {body[:160]!r}")
        srv.signal()
        if srv.wait() != 0:
            problems.append("the server did not stop cleanly")
    for p in problems[:20]:
        print("FAIL", p)
    cats = ", ".join(f"{k} {v}" for k, v in sorted(seen.items()))
    print(f"web_fuzz: {count} requests ({cats}), {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
