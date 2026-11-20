#!/usr/bin/env python3
"""Spec 028 SC-002: throughput scales with workers. A handler that computes (no waiting) serves 2,000
requests from 8 clients with 1 worker and with 4; 4 workers must be at least 3 times faster. Skipped,
with a message, on machines with fewer than 4 cores. Part of `make gate`."""
import concurrent.futures
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serve_workers import HEAD, Server, get  # noqa: E402

PROGRAM = HEAD + """
fn setup() -> result[int] {
    return ok(0);
}

fn handle(n: int, r: Request) -> Response {
    var x = 0;
    for i in 0..200000 {
        set x = (x * 31 + i) % 1000003;
    }
    return new Response { status: 200, headers: new list[Header], body: str(x) };
}

fn main() -> result[void] uses serve("PORT") {
    return serve_workers("PORT", 1, setup, handle);
}
"""


def rate(workers):
    """Requests per second with that many workers."""
    with Server(PROGRAM, args=("--workers", str(workers))) as srv:
        for _ in range(20):  # warm up
            get(srv.port, "/")
        t = time.monotonic()
        with concurrent.futures.ThreadPoolExecutor(8) as pool:
            codes = list(pool.map(lambda i: get(srv.port, "/")[0], range(2000)))
        took = time.monotonic() - t
        srv.signal()
        srv.wait()
    if codes.count(200) != 2000:
        sys.exit("FAIL %d of 2000 requests not answered with %d workers" % (2000 - codes.count(200), workers))
    return 2000 / took


def main():
    if (os.cpu_count() or 1) < 4:
        print("serve_workers_speed: skipped (%d cores; needs 4)" % (os.cpu_count() or 1))
        return 0
    one, four = rate(1), rate(4)
    ratio = four / one
    print("serve_workers_speed: 1 worker %.0f req/s, 4 workers %.0f req/s: %.2fx (want >= 3)" % (one, four, ratio))
    return 0 if ratio >= 3 else 1


if __name__ == "__main__":
    sys.exit(main())
