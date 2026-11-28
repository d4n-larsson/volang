#!/usr/bin/env python3
"""Spec 054: recorded tests (`vo test --record DIR` / `--replay DIR`) beyond the step cases.

  recorded_tests.py [--quick] [--group NAME]

tcp      a test that talks to tests/tcp_server.py (echo) through std.tcp is recorded, the server is
         stopped, and the replayed test passes (US1, scenario 5); other bytes sent are R0014
nonet    SC-002: recorded HTTP tests replay with no network at all (`unshare -rn`; skipped where not
         permitted)
speed    SC-003: a test of three requests to a server that answers after 100 ms: the replayed test takes
         under 1% of the live one (the tests' own times, median of 5)
stable   SC-005: 20 tests recorded twice against the unchanged server give identical directories
redact   SC-006: generated secrets in headers, query parameters and variables appear in no recording;
         the replay passes
mutate   SC-007: `vo test --replay --mutate` on examples/rates runs offline (no network) and reports
--quick  runs tcp, stable (5 tests) and redact
"""
import json
import os
import random
import shutil
import statistics
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
TCP_SERVER = os.path.join(ROOT, "tests", "tcp_server.py")
HTTP_SERVER = os.path.join(ROOT, "tests", "http_server.py")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1

failures = []
cases = 0


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def vo(args, cwd, env=None):
    e = dict(os.environ)
    e.update(env or {})
    p = subprocess.run([VO] + args, cwd=cwd, capture_output=True, text=True, env=e, timeout=300 * SLOW)
    return p.returncode, p.stdout + p.stderr


class Peer:
    """A tests/tcp_server.py process; .port once it listens."""

    def __init__(self, mode):
        self.p = subprocess.Popen([sys.executable, TCP_SERVER, mode], stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
        self.port = int(self.p.stdout.readline().split()[1])

    def stop(self):
        self.p.kill()
        self.p.wait()


TCP_PROGRAM = """import std.bytes;
import std.tcp;

fn port() -> int uses env {
    match env("PORT") {
        case ok(p) {
            match parse_int(p) {
                case ok(n) {
                    return n;
                }
                case err(_) {
                }
            }
        }
        case err(_) {
        }
    }
    return 1;
}

fn echo(text: str) -> str uses env, net {
    match tcp.connect("127.0.0.1", port(), 2000) {
        case ok(c) {
            match tcp.write_all(c, bytes.from_text(text), 2000) {
                case ok() {
                }
                case err(_) {
                    return "send failed";
                }
            }
            match tcp.read(c, 100, 2000) {
                case ok(b) {
                    tcp.close_conn(c);
                    match bytes.to_text(b) {
                        case ok(t) {
                            return t;
                        }
                        case err(_) {
                            return "not text";
                        }
                    }
                }
                case err(_) {
                    return "read failed";
                }
            }
        }
        case err(_) {
            return "connect failed";
        }
    }
}

fn main() {
}

test "a peer echoes" uses env, net {
    expect_eq(echo("ping"), "ping");
}
"""


def group_tcp(d):
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(TCP_PROGRAM)
    peer = Peer("echo")
    try:
        code, out = vo(["test", "--record", "rec", "main.vo"], d, {"PORT": str(peer.port)})
    finally:
        peer.stop()
    case("tcp/recording passes against the peer", code == 0 and "ms, recorded)" in out, out[-400:])
    rec = os.path.join(d, "rec", "main", "a_peer_echoes.jsonl")
    case("tcp/the recording holds the exchange", os.path.isfile(rec) and '"tcp_send"' in open(rec).read(), rec)
    code, out = vo(["test", "--replay", "rec", "main.vo"], d)
    case("tcp/replay passes with the peer gone", code == 0 and "replayed" in out, out[-400:])
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(TCP_PROGRAM.replace('echo("ping"), "ping"', 'echo("pong"), "ping"'))
    code, out = vo(["test", "--replay", "rec", "main.vo"], d)
    case("tcp/other bytes sent are R0014 naming the connection", code != 0 and "R0014" in out and "sends other bytes to `127.0.0.1:" in out, out[-600:])


class Http:
    """A tests/http_server.py process; .port once it listens."""

    def __init__(self):
        self.p = subprocess.Popen([sys.executable, HTTP_SERVER], stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
        self.port = int(self.p.stdout.readline().split()[1])

    def stop(self):
        self.p.kill()
        self.p.wait()


HEAD = """import std.http;

fn base() -> str uses env {
    match env("PORT") {
        case ok(p) {
            return "http://127.0.0.1:" + p;
        }
        case err(_) {
            return "http://127.0.0.1:1";
        }
    }
}

fn body_at(path: str) -> str uses env, net {
    match http.get(base() + path) {
        case ok(r) {
            return r.body;
        }
        case err(e) {
            return "error: " + e;
        }
    }
}

fn main() {
}
"""


def program(tests):
    return HEAD + "".join(f'\ntest "{name}" uses env, net {{\n{body}}}\n' for name, body in tests)


def record_then(d, tests, env=None, extra=(), srv=None):
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(program(tests))
    own = srv is None
    srv = srv or Http()
    try:
        e = {"PORT": str(srv.port)}
        e.update(env or {})
        return vo(["test", "--record", "rec", *extra, "main.vo"], d, e)
    finally:
        if own:
            srv.stop()


def group_nonet(d):
    if subprocess.run(["unshare", "-rn", "true"], capture_output=True).returncode != 0:
        print("recorded_tests: nonet skipped (no network namespaces)")
        return
    code, out = record_then(d, [("plain", '    expect_eq(body_at("/plain"), "hello from vo\\n");\n'),
                                ("json", '    expect(len(body_at("/json")) > 0);\n')])
    case("nonet/recorded", code == 0, out[-300:])
    p = subprocess.run(["unshare", "-rn", VO, "test", "--replay", "rec", "main.vo"], cwd=d, capture_output=True, text=True)
    case("nonet/replay passes with no network (SC-002)", p.returncode == 0 and "2 passed" in p.stdout, p.stdout + p.stderr)


def group_speed(d):
    body = "".join('    expect(len(body_at("/delay/100")) > 0);\n' for _ in range(3))
    code, out = record_then(d, [("slow service", body)])
    case("speed/recorded", code == 0, out[-300:])
    srv = Http()
    try:
        live, replayed = [], []
        for _ in range(5):
            for runs, args, env in ((live, ["test", "--json", "main.vo"], {"PORT": str(srv.port)}),
                                    (replayed, ["test", "--replay", "rec", "--json", "main.vo"], {})):
                _, o = vo(args, d, env)
                ms = [json.loads(l)["ms"] for l in o.splitlines() if l.startswith('{"v":1,"kind":"test"')]
                runs.append(ms[0] if ms else 10 ** 6)
    finally:
        srv.stop()
    ml, mr = statistics.median(live), statistics.median(replayed)
    print(f"recorded_tests: speed: live {ml} ms, replayed {mr} ms")
    case("speed/replay under 1% of live (SC-003)", mr <= 0.01 * ml, f"{mr} against {ml}")


def group_stable(d, n=20):
    paths = ["/plain", "/json", "/headers", "/status/404"] + [f"/hex/{i:02x}{i + 1:02x}" for i in range(40)]
    tests = [(f"t{i:02d}", f'    expect(len(body_at("{paths[i % len(paths)]}")) >= 0);\n') for i in range(n)]
    srv = Http()  # one server for both: the same port, the same answers
    try:
        code, out = record_then(d, tests, srv=srv)
        case("stable/first recording", code == 0, out[-300:])
        first = os.path.join(d, "first")
        shutil.copytree(os.path.join(d, "rec"), first)
        code, out = record_then(d, tests, srv=srv)
    finally:
        srv.stop()
    same = all(open(os.path.join(first, "main", f), "rb").read() == open(os.path.join(d, "rec", "main", f), "rb").read()
               for f in os.listdir(os.path.join(first, "main")))
    count = len(os.listdir(os.path.join(first, "main")))
    case(f"stable/{count} tests recorded twice are identical (SC-005)", code == 0 and same and count == n,
         f"{count} files, same {same}")


def group_redact(d):
    rng = random.Random(54)
    secret = lambda: "".join(rng.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(16))
    vals = {"auth": secret(), "key": secret(), "q": secret(), "env": secret(), "cookie": secret()}
    body = f"""    let headers = [
        new http.Header {{ name: "Authorization", value: "Bearer {vals["auth"]}" }},
        new http.Header {{ name: "Proxy-Authorization", value: "Basic {vals["auth"]}" }},
        new http.Header {{ name: "x-api-key", value: "{vals["key"]}" }},
        new http.Header {{ name: "Cookie", value: "s={vals["cookie"]}" }}
    ];
    let secret = env("SERVICE_SECRET");
    expect(secret == ok("{vals["env"]}") or secret == ok("[redacted]"));
    match http.send("GET", base() + "/cookie?token={vals["q"]}&page=1", headers, "", 2000) {{
        case ok(r) {{
            expect_eq(r.status, 200);
        }}
        case err(e) {{
            expect_eq(e, "");
        }}
    }}
"""
    tests = [("redacts", body)]
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(program(tests))
    srv = Http()
    try:
        code, out = vo(["test", "--record", "rec", "--redact", "X-Api-Key", "--redact", "token", "--redact", "SERVICE_SECRET", "main.vo"], d,
                       {"PORT": str(srv.port), "SERVICE_SECRET": vals["env"]})
    finally:
        srv.stop()
    case("redact/recorded", code == 0, out[-400:])
    text = open(os.path.join(d, "rec", "main", "redacts.jsonl")).read()
    found = [k for k, v in vals.items() if v in text]
    case("redact/no secret in the recording (SC-006)", not found, str(found))
    code, out = vo(["test", "--replay", "rec", "main.vo"], d)
    case("redact/the replay passes", code == 0, out[-400:])


def group_mutate(d):
    ex = os.path.join(ROOT, "examples", "rates")
    cmd = [VO, "test", "--replay", "recordings/tests", "--mutate", "src/main.vo"]
    if subprocess.run(["unshare", "-rn", "true"], capture_output=True).returncode == 0:
        cmd = ["unshare", "-rn"] + cmd
    p = subprocess.run(cmd, cwd=ex, capture_output=True, text=True, timeout=600 * SLOW)
    line = [l for l in p.stdout.splitlines() if l.startswith("mutation:")]
    print("recorded_tests: mutate:", line[0] if line else p.stdout[-200:])
    case("mutate/mutation testing runs offline on the example (SC-007)", bool(line) and "not tried" in line[0], p.stdout[-400:] + p.stderr[-200:])


GROUPS = {"tcp": group_tcp, "nonet": group_nonet, "speed": group_speed, "stable": group_stable, "redact": group_redact,
          "mutate": group_mutate}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    if "--quick" in args and "--group" not in args:
        names = ["tcp", "stable", "redact"]
        GROUPS["stable"] = lambda d: group_stable(d, 5)
    tmp = os.path.join(ROOT, "build", "tmp")
    os.makedirs(tmp, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=tmp) as base:
        for n in names:
            sub = os.path.join(base, n)
            os.makedirs(sub)
            GROUPS[n](sub)
    print(f"recorded_tests: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
