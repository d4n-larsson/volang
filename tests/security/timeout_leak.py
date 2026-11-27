#!/usr/bin/env python3
"""Bug sweep 2026-11-27 (finding 17): a handler whose HTTP call hits --timeout must not leave its
socket open. A server run with `--timeout 1` calls a peer that accepts and never answers; after 5
such requests (each a 500) the server has the descriptors it started with. Exit 1 on a leak.

  timeout_leak.py [VO]
"""
import os, socket, subprocess, sys, tempfile, time, urllib.error, urllib.request

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


silent = socket.socket()  # accepts (the backlog) and never answers
silent.bind(("127.0.0.1", 0))
silent.listen(64)
peer, port = silent.getsockname()[1], free_port()
PROGRAM = """import std.http;

fn main() -> result[void] uses serve("PORT"), net("127.0.0.1") {
    try http.serve("PORT", fn(r) {
        match http.get("http://127.0.0.1:PEER/") {
            case ok(x) {
                return new http.Response { status: 200, headers: new list[http.Header], body: "ok" };
            }
            case err(e) {
                return new http.Response { status: 502, headers: new list[http.Header], body: e };
            }
        }
    });
    return ok();
}
""".replace("PORT", str(port)).replace("PEER", str(peer))
d = tempfile.mkdtemp()
open(os.path.join(d, "main.vo"), "w").write(PROGRAM)
p = subprocess.Popen([VO, "run", "--timeout", "1", "main.vo"], cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=1).close()
            break
        except OSError:
            time.sleep(0.05)
    time.sleep(0.2)
    fds = lambda: len(os.listdir(f"/proc/{p.pid}/fd"))
    before, statuses = fds(), []
    for _ in range(5):
        try:
            statuses.append(urllib.request.urlopen(f"http://127.0.0.1:{port}/", timeout=10).status)
        except urllib.error.HTTPError as e:
            statuses.append(e.code)
    time.sleep(0.2)
    after = fds()
finally:
    p.kill()
    p.wait()
    subprocess.run(["rm", "-rf", d])
ok = statuses == [500] * 5 and after == before
print(f"timeout_leak: statuses {statuses}, descriptors {before} -> {after}{'' if ok else ' (LEAK or wrong status)'}")
sys.exit(0 if ok else 1)
