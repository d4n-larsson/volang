#!/usr/bin/env python3
"""Spec 029 SC-006: what routing and binding cost in throughput. A 20-route API with std.web and the
same API written by hand on std.http (a route loop, path splitting, int parsing, JSON by hand), both
served by one worker (the worker's work is what differs; more workers only add the server process's
share), measured with ab (5,000 requests, 16 at once, to the last route).

The two are run in turns, 9 rounds, the order alternating, and the figure is the median of the per-round
ratios, so a machine whose speed drifts during the check slows both of a round alike. std.web must reach
at least 80% of the hand-written throughput. The cost is a fixed 5 to 8 microseconds per request, so the
ratio depends on how fast the machine serves a request at all: 87% to 92% on the development machine (10,000
requests a second), 84% on a GitHub runner (40,000). The bound was 90% until November 2026; it is there
to catch routing that got slower, not to compare machines. Skipped with a message without ab, and on
CI (the variable `CI` is set): two runs on GitHub's runners gave 84% and 69% for the same code path. Part
of `make gate` on a developer's machine."""
import os
import re
import shutil
import statistics
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serve_workers import Server  # noqa: E402

N = 20

WEB = """import std.http;
import std.web;

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
fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses serve("PORT") {
    let routes: list[Route[int]] = [""" + ", ".join(f'on_get("/r{i}/items/{{id}}", r{i})' for i in range(N)) + """];
    return serve_app("PORT", 1, setup, new App { routes: routes, steps: new list[Step[int]], errors: new list[ErrorStatus] });
}
"""

HAND = """import std.http;

fn answer(status: int, body: str) -> http.Response {
    return new http.Response { status: status, headers: [new http.Header { name: "Content-Type", value: "application/json" }], body: body };
}

fn handle(n: int, r: http.Request) -> http.Response {
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

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses serve("PORT") {
    return serve_workers("PORT", 1, setup, handle);
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
    if os.environ.get("CI"):  # a shared runner: 84% in one run and 69% in the next, where an idle machine is steady
        print("web_speed: skipped on CI (a throughput ratio on a shared machine; run it locally)")
        return 0
    hands, webs = [], []
    for r in range(9):
        if r % 2:
            webs.append(rate(WEB))
            hands.append(rate(HAND))
        else:
            hands.append(rate(HAND))
            webs.append(rate(WEB))
    hand, web = statistics.median(hands), statistics.median(webs)
    ratio = statistics.median(w / h for w, h in zip(webs, hands))
    print(f"web_speed: hand-written {hand:.0f} req/s, std.web {web:.0f} req/s: {ratio:.0%} (want >= 80%)")
    return 0 if ratio >= 0.8 else 1


if __name__ == "__main__":
    sys.exit(main())
