#!/usr/bin/env python3
"""Spec 011 SC-003: a vo server keeps answering under the default --timeout.

Starts a small JSON service (routes, a failing route every 100th request, a 404), sends requests one
after another until both --requests N (default 10000) and --minutes M (default 0) are reached, checks
every response, samples the server's resident memory after the first 10% of the requests and at the
end, then stops it with SIGTERM: it must exit 0. Exit 1 on any wrong answer, on memory growing by more
than 50% (plus 8 MiB) after warm-up, or on a bad stop.

  serve_endurance.py [--requests N] [--minutes M] [--pause MS]   (MS between requests, for long runs:
                                                                  closed connections wait in TIME_WAIT)
  serve_endurance.py --stalled [--requests N]   (spec 016 SC-002: one client holds half a request open
                                                 while N requests (default 100) are sent; each must be
                                                 answered within 1 s)
"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

PROGRAM = """import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve;
import std.json.Json;
import std.json.write_json;

fn handle(r: Request) -> Response {
    if r.path == "/fail" {
        let zero = len(r.query) - len(r.query);
        return new Response { status: 200, headers: new list[Header], body: str(1 / zero) };
    }
    if r.path != "/count" {
        return new Response { status: 404, headers: new list[Header], body: "" };
    }
    let items = map(split(r.query, ","), fn(s: str) -> Json {
        return Json.text(s);
    });
    match write_json(Json.array(items)) {
        case ok(text) {
            return new Response { status: 200, headers: [new Header { name: "Content-Type", value: "application/json" }], body: text };
        }
        case err(e) {
            return new Response { status: 500, headers: new list[Header], body: e };
        }
    }
}

fn main() -> result[void] uses console, serve("PORT") {
    try serve("PORT", handle);
    print("stopped");
    return ok();
}
"""


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def exchange(port, data):
    with socket.create_connection(("127.0.0.1", port), timeout=30) as s:
        s.sendall(data)
        s.shutdown(socket.SHUT_WR)
        out = b""
        while True:
            b = s.recv(65536)
            if not b:
                return out
            out += b


def rss_kib(pid):
    """VmRSS of the process, or of its first child (bin/vo may run the binary under qemu)."""
    for p in [pid] + [int(c) for c in open(f"/proc/{pid}/task/{pid}/children").read().split()]:
        for line in open(f"/proc/{p}/status"):
            if line.startswith("VmRSS:"):
                kib = int(line.split()[1])
                if kib > 2048:
                    return kib
    return 0


def main():
    args = sys.argv[1:]
    n = int(args[args.index("--requests") + 1]) if "--requests" in args else 10000
    minutes = float(args[args.index("--minutes") + 1]) if "--minutes" in args else 0
    pause = float(args[args.index("--pause") + 1]) / 1000 if "--pause" in args else 0
    port = free_port()
    work = tempfile.mkdtemp()
    open(os.path.join(work, "main.vo"), "w").write(PROGRAM.replace("PORT", str(port)))
    out_f, err_f = tempfile.TemporaryFile(), tempfile.TemporaryFile()  # not pipes: reports would fill one
    proc = subprocess.Popen([VO, "run", "main.vo"], cwd=work, stdout=out_f, stderr=err_f)
    for _ in range(600):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=1).close()
            break
        except OSError:
            time.sleep(0.05)
    stalled = None
    if "--stalled" in args:  # spec 016: a client that sends half a request and waits
        n = int(args[args.index("--requests") + 1]) if "--requests" in args else 100
        stalled = socket.create_connection(("127.0.0.1", port), timeout=30)
        stalled.sendall(b"GET /count?a HTTP/1.1\r\nHost: lo")
    slowest = 0.0
    start, done, bad, warm = time.time(), 0, 0, None
    while done < n or time.time() - start < minutes * 60:
        i = done
        t0 = time.time()
        if i % 100 == 99:
            got = exchange(port, b"GET /fail HTTP/1.1\r\n\r\n")
            ok = got.startswith(b"HTTP/1.1 500 ")
        elif i % 100 == 50:
            got = exchange(port, b"GET /nope HTTP/1.1\r\n\r\n")
            ok = got.startswith(b"HTTP/1.1 404 ")
        else:
            got = exchange(port, f"GET /count?a,{i},c HTTP/1.1\r\n\r\n".encode())
            ok = got.startswith(b"HTTP/1.1 200 ") and got.endswith(f'["a","{i}","c"]'.encode())
        slowest = max(slowest, time.time() - t0)
        if not ok:
            bad += 1
            if bad <= 3:
                print(f"  request {i}: unexpected {got[:120]!r}")
        done += 1
        if pause:
            time.sleep(pause)
        if done == max(1, n // 10):
            warm = rss_kib(proc.pid)
    end_rss = rss_kib(proc.pid)
    if stalled:
        stalled.close()
    elapsed = time.time() - start
    proc.send_signal(signal.SIGTERM)
    proc.wait(timeout=60)
    out_f.seek(0)
    err_f.seek(0)
    out, err = out_f.read(), err_f.read()
    failures = err.count(b"runtime error[R0002]")
    problems = []
    if bad:
        problems.append(f"{bad} wrong responses")
    if failures != done // 100:
        problems.append(f"{failures} handler failures reported, {done // 100} expected")
    if proc.returncode != 0 or out != b"stopped\n":
        problems.append(f"stop: exit {proc.returncode}, stdout {out!r}")
    if stalled and slowest > 1.0:
        problems.append(f"the slowest request took {slowest:.2f} s behind a stalled client (limit 1 s)")
    if warm and end_rss > warm * 1.5 + 8192:
        problems.append(f"memory grew from {warm} KiB to {end_rss} KiB")
    print(f"serve_endurance: {done} requests in {elapsed:.0f} s ({done / elapsed:.0f}/s), "
          f"memory {warm} KiB after warm-up, {end_rss} KiB at the end, "
          f"slowest {slowest * 1000:.0f} ms{' behind a stalled client' if stalled else ''}, "
          f"{'0 problems' if not problems else '; '.join(problems)}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
