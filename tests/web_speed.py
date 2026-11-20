#!/usr/bin/env python3
"""Spec 029 SC-006: routing and binding cost at most 10% of throughput. A 20-route API with std.web and
the same API written by hand on std.http (a route loop, path splitting, int parsing, JSON by hand), both
served by one worker (the worker's work is what differs; more workers only add the server process's
share), measured with ab (5,000 requests, 16 at once, to the last route; the median of 5 interleaved
runs each); std.web must
reach at least 90% of the hand-written throughput. Skipped with a message without ab. Part of `make gate`."""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serve_workers import Server  # noqa: E402

N = 20

WEB = """import std.http.Response;
import std.web.App;
import std.web.ErrorStatus;
import std.web.Route;
import std.web.Step;
import std.web.json_reply;
import std.web.on_get;
import std.web.serve_app;

record Key {
    id: int;
}

record Answer {
    route: int;
    id: int;
}

""" + "\n".join(f"""fn r{i}(n: int, input: Key) -> result[Response] {{
    return ok(json_reply(200, new Answer {{ route: {i}, id: input.id }}));
}}
""" for i in range(N)) + """
fn main() -> result[void] uses serve("PORT") {
    let routes: list[Route[int]] = [""" + ", ".join(f'on_get("/r{i}/items/{{id}}", r{i})' for i in range(N)) + """];
    return serve_app("PORT", 1, fn() -> result[int] { return ok(0); }, new App { routes: routes, steps: new list[Step[int]], errors: new list[ErrorStatus] });
}
"""

HAND = """import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve_workers;

fn answer(status: int, body: str) -> Response {
    return new Response { status: status, headers: [new Header { name: "Content-Type", value: "application/json" }], body: body };
}

fn handle(n: int, r: Request) -> Response {
    let parts = split(r.path, "/");
    if len(parts) != 4 or parts[2] != "items" {
        return answer(404, "{\\"status\\":404}");
    }
    for i in 0..""" + str(N) + """ {
        if parts[1] == "r" + str(i) {
            if r.method != "GET" {
                return answer(405, "{\\"status\\":405}");
            }
            match parse_int(parts[3]) {
                case ok(id) {
                    return answer(200, "{\\"route\\":" + str(i) + ",\\"id\\":" + str(id) + "}");
                }
                case err(_) {
                    return answer(400, "{\\"status\\":400}");
                }
            }
        }
    }
    return answer(404, "{\\"status\\":404}");
}

fn main() -> result[void] uses serve("PORT") {
    return serve_workers("PORT", 1, fn() -> result[int] { return ok(0); }, handle);
}
"""


def rate(program):
    with Server(program) as srv:
        url = f"http://127.0.0.1:{srv.port}/r{N - 1}/items/42"
        subprocess.run(["ab", "-q", "-n", "500", "-c", "16", url], capture_output=True)  # warm up
        out = subprocess.run(["ab", "-q", "-n", "5000", "-c", "16", url], capture_output=True, text=True).stdout
        srv.signal()
        srv.wait()
    failed = re.search(r"Failed requests:\s+(\d+)", out)
    if not failed or failed.group(1) != "0" or "Non-2xx" in out:
        sys.exit("FAIL web_speed: requests failed\n" + out)
    return float(re.search(r"Requests per second:\s+([\d.]+)", out).group(1))


def main():
    if not shutil.which("ab"):
        print("web_speed: skipped (no ab)")
        return 0
    hands, webs = [], []
    for _ in range(5):  # interleaved; medians (single runs vary by 20% on a busy machine)
        hands.append(rate(HAND))
        webs.append(rate(WEB))
    hand, web = sorted(hands)[2], sorted(webs)[2]
    ratio = web / hand
    print(f"web_speed: hand-written {hand:.0f} req/s, std.web {web:.0f} req/s: {ratio:.0%} (want >= 90%)")
    return 0 if ratio >= 0.9 else 1


if __name__ == "__main__":
    sys.exit(main())
