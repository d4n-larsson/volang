#!/usr/bin/env python3
"""Spec 036: `vo mcp`, checked by a scripted client (specs/036-vo-mcp/contracts/mcp.md).

- equivalence: every tool's result (stdout block, stderr block, exit status) equals the direct
  command's with --json, run in an identical copy of the same small project;
- validation: bad arguments are -32602 naming the argument, and nothing runs;
- isolation: a timeout, memory exhaustion, exit, a runtime error, a flood of output, a cancelled call,
  a host that goes away; nothing is left running and the next call is unaffected;
- protocol: malformed messages get JSON-RPC's errors and the server carries on.

  mcp.py                 the checks (part of `make test`)
  mcp.py --session N     SC-002: the server's memory after N mixed calls
  mcp.py --time          SC-003: a call against the direct command (timing-sensitive)
  mcp.py --fuzz N        N random and damaged lines; the server must answer a final ping
"""
import difflib
import json
import os
import random
import re
import select
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN (default build/vo) natively or under qemu
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1  # under qemu

FILES = {
    "main.vo": '''record Item {
    name: str;
    price: int;
}

fn total(items: list[Item]) -> int {
    var sum = 0;
    for it in items {
        set sum = sum + it.price;
    }
    return sum;
}

fn main() uses console, input {
    let items = [new Item { name: "pen", price: 3 }];
    print("total " + str(total(items)));
    print("args: " + join(args(), "|"));
    print("stdin: " + read_all());
}

test "total" {
    expect_eq(total([new Item { name: "a", price: 2 }]), 2);
}

test "fails" {
    expect_eq(total(new list[Item]), 1);
}
''',
    "rand.vo": 'fn main() uses console, random, clock {\n'
               '    print(str(random_int(1, 1000000)) + " " + str(now_ms()));\n}\n',
    "bad.vo": 'fn main() uses console {\n    let x: int = "text";\n    print(str(x));\n}\n',
    "ugly.vo": 'fn main() uses console{\n  print("x");\n}\n',
    "loop.vo": "fn main() {\n    var i = 0;\n    while true {\n        set i = i + 1;\n    }\n}\n",
    "alloc.vo": "fn main() {\n    let xs = new list[str];\n    while true {\n"
                '        push(xs, "0123456789012345678901234567890123456789" + str(len(xs)));\n    }\n}\n',
    "exit3.vo": 'fn main() uses console {\n    print("before");\n    exit(3);\n}\n',
    "rterr.vo": "fn main() uses console {\n    let xs = [1];\n    print(str(xs[len(xs) + 4]));\n}\n",
    "flood.vo": '''fn main() uses console {
    var line = "0123456789abcdef";
    while len(line) < 1024 {
        set line = line + line;
    }
    for _ in 0..3072 {
        if len(args()) > 0 {
            eprint(slice(line, 0, 1023));
        } else {
            print(slice(line, 0, 1023));
        }
    }
}
''',
    "raw.vo": "import std.bytes.from_ints;\nimport std.bytes.write_stdout;\n\n"
              "fn main() uses console {\n    write_stdout(from_ints([104, 255, 254, 105]));\n}\n",
}
EDITED_TOTAL = FILES["main.vo"].replace("set sum = sum + it.price;", "set sum = sum + it.price + 0;")
# the diff describes a change already made: from an older text to the file as it is
DIFF = "".join(difflib.unified_diff(EDITED_TOTAL.splitlines(True), FILES["main.vo"].splitlines(True),
                                    "a/main.vo", "b/main.vo"))

TOOLS = {  # contracts/mcp.md: tool -> (arguments, required, hint)
    "check": ("paths", "paths", "r"),
    "run": ("file args stdin timeout seed clock_ms max_memory mem_stats ca_files workers", "file", "x"),
    "test": ("paths filter tests specs diff mode max_mutants timeout seed clock_ms max_memory mem_stats ca_files",
             "paths", "x"),
    "fmt": ("paths check", "paths", "w"),
    "effects": ("file update", "file", "w"),
    "explain": ("code", "code", "r"),
    "doc": ("entry name", "", "r"),
    "query": ("question entry target", "question entry", "r"),
    "impact": ("entry diff", "entry diff", "r"),
    "edit": ("edit entry target change dry_run", "edit entry target change", "w"),
}

fails = []


def fail(case, why):
    fails.append(case)
    print(f"FAIL {case}: {why}")


def project(work, name):
    d = os.path.join(work, name)
    os.mkdir(d)
    for f, text in FILES.items():
        open(os.path.join(d, f), "w").write(text)
    return d


def direct(args, cwd, stdin=b""):
    r = subprocess.run([VO] + args, cwd=cwd, input=stdin, capture_output=True, timeout=120 * SLOW)
    return r.stdout, r.stderr, r.returncode


class Server:
    def __init__(self, cwd, init=True):
        self.cwd = cwd
        self.p = subprocess.Popen([VO, "mcp"], cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.buf = b""
        self.next = 100
        if init:
            self.req("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                    "clientInfo": {"name": "tests", "version": "0"}})
            self.note("notifications/initialized")

    def raw(self, data):
        self.p.stdin.write(data)
        self.p.stdin.flush()

    def send(self, obj):
        self.raw(json.dumps(obj).encode() + b"\n")

    def note(self, method, params=None):
        self.send({"jsonrpc": "2.0", "method": method, **({"params": params} if params is not None else {})})

    def line(self, timeout=60):
        """The next output line, parsed; None at a timeout or end of output."""
        end = time.time() + timeout * SLOW
        while b"\n" not in self.buf:
            left = end - time.time()
            if left <= 0 or not select.select([self.p.stdout], [], [], left)[0]:
                return None
            chunk = os.read(self.p.stdout.fileno(), 1 << 20)
            if not chunk:
                return None
            self.buf += chunk
        raw, self.buf = self.buf.split(b"\n", 1)
        msg = json.loads(raw.decode("utf-8"))  # raises on anything that is not one JSON message
        assert msg.get("jsonrpc") == "2.0", raw[:200]
        return msg

    def ask(self, method, params=None):
        self.next += 1
        self.send({"jsonrpc": "2.0", "id": self.next, "method": method,
                   **({"params": params} if params is not None else {})})
        return self.next

    def req(self, method, params=None, timeout=60):
        i = self.ask(method, params)
        msg = self.line(timeout)
        assert msg is not None, f"no answer to {method}"
        assert msg.get("id") == i, f"answer to {method} has id {msg.get('id')}, not {i}"
        return msg

    def call(self, tool, arguments, timeout=60):
        """(stdout, stderr-or-stdout blocks, exit record, isError) or the error object."""
        msg = self.req("tools/call", {"name": tool, "arguments": arguments}, timeout)
        if "error" in msg:
            return msg["error"]
        blocks = [b["text"] for b in msg["result"]["content"]]
        assert all(b["type"] == "text" for b in msg["result"]["content"])
        return blocks[:-1], json.loads(blocks[-1]), msg["result"]["isError"]

    def close(self, timeout=10):
        try:
            self.p.stdin.close()
        except BrokenPipeError:
            pass
        try:
            code = self.p.wait(timeout * SLOW)
        except subprocess.TimeoutExpired:
            self.p.kill()
            code = "no exit"
        self.p.stdout.close()
        return code


def others_in(cwd, server_pid):
    """Live processes (not zombies) whose working directory is cwd, apart from the server."""
    found = []
    for pid in os.listdir("/proc"):
        if not pid.isdigit() or int(pid) == server_pid:
            continue
        try:
            if os.readlink(f"/proc/{pid}/cwd") == cwd and open(f"/proc/{pid}/stat").read().split(") ")[-1][0] != "Z":
                found.append(int(pid))
        except OSError:
            pass
    return found


def gone(cwd, server_pid, seconds=3):
    end = time.time() + seconds * SLOW
    while time.time() < end:
        if not others_in(cwd, server_pid):
            return True
        time.sleep(0.05)
    return False


# ---- the cases ----

def protocol_basics(work):
    d = project(work, "basics")
    for asked, got in (("2025-06-18", "2025-06-18"), ("2025-03-26", "2025-03-26"), ("2024-11-05", "2024-11-05"),
                       ("1999-01-01", "2025-06-18")):
        s = Server(d, init=False)
        if s.req("ping").get("result") != {}:
            fail("ping before initialize", "not answered with {}")
        r = s.req("initialize", {"protocolVersion": asked, "capabilities": {}})["result"]
        version = direct(["version"], d)[0].decode().split()[1]
        if r["protocolVersion"] != got:
            fail(f"initialize {asked}", f"answered {r['protocolVersion']}, expected {got}")
        if r["serverInfo"] != {"name": "vo", "version": version} or set(r["capabilities"]) != {"tools", "resources"}:
            fail(f"initialize {asked}", f"serverInfo or capabilities: {r}")
        s.note("notifications/initialized")
        s.note("notifications/whatever", {"x": 1})
        if s.req("ping").get("result") != {}:
            fail("ping", "not answered with {}")
        if s.close() != 0:
            fail("end of input", "exit status is not 0")
    out = subprocess.run([VO, "mcp"], cwd=d, capture_output=True, timeout=30 * SLOW, input=b"".join(
        json.dumps(m).encode() + b"\n" for m in (
            {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
            {"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "check", "arguments": {"paths": ["bad.vo"]}}},
            {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "run", "arguments": {"file": "exit3.vo"}}})) +
        b'{"jsonrpc":"2.0","id":4,"method":"ping"}')  # the last line has no newline
    ids = [json.loads(line)["id"] for line in out.stdout.splitlines()]
    if ids != [1, 2, 3, 4] or out.returncode != 0 or out.stderr:
        fail("piped session", f"ids {ids}, exit {out.returncode}, stderr {out.stderr[:200]!r}")


def tool_list(work):
    s = Server(project(work, "list"))
    tools = s.req("tools/list")["result"]["tools"]
    if [t["name"] for t in tools] != list(TOOLS):
        fail("tools/list", f"names {[t['name'] for t in tools]}")
    for t in tools:
        args, required, hint = TOOLS.get(t["name"], ("", "", ""))
        sch = t["inputSchema"]
        if sorted(sch["properties"]) != sorted(args.split()) or sorted(sch["required"]) != sorted(required.split()):
            fail(f"tools/list {t['name']}", f"properties {sorted(sch['properties'])}, required {sch['required']}")
        if sch["type"] != "object" or sch["additionalProperties"] is not False or not t["description"]:
            fail(f"tools/list {t['name']}", "schema shape or description")
        if any(not p.get("description") or "type" not in p for p in sch["properties"].values()):
            fail(f"tools/list {t['name']}", "a property without a type or description")
        a = t["annotations"]
        want = {"r": (True, False), "w": (False, False), "x": (False, True)}[hint]
        if (a["readOnlyHint"], a["openWorldHint"]) != want or (hint != "r" and a.get("destructiveHint") is not True):
            fail(f"tools/list {t['name']}", f"annotations {a}")
    s.close()


def equivalence(work):
    da, db = project(work, "eq-a"), project(work, "eq-b")
    difffile = os.path.join(work, "change.diff")
    open(difffile, "w").write(DIFF)
    s = Server(da)
    cases = [  # tool, arguments, the direct command's words, its stdin
        ("check", {"paths": ["exit3.vo"]}, ["check", "--json", "exit3.vo"]),
        ("check", {"paths": ["bad.vo", "exit3.vo"]}, ["check", "--json", "bad.vo", "exit3.vo"]),
        ("check", {"paths": ["."]}, ["check", "--json", "."]),
        ("check", {"paths": ["missing.vo"]}, ["check", "--json", "missing.vo"]),
        ("run", {"file": "main.vo"}, ["run", "--json", "main.vo"]),
        ("run", {"file": "main.vo", "args": ["a", "--seed", "--", "b c"], "stdin": "typed\niné\u0000x"},
         ["run", "--json", "main.vo", "--", "a", "--seed", "--", "b c"], "typed\niné\u0000x".encode()),
        ("run", {"file": "main.vo", "args": []}, ["run", "--json", "main.vo"]),
        ("run", {"file": "rand.vo", "seed": 7, "clock_ms": 1700000000000, "timeout": 5, "max_memory": 64,
                 "mem_stats": True}, ["run", "--json", "--timeout", "5", "--seed", "7", "--clock-ms", "1700000000000",
                                      "--max-memory", "64", "--mem-stats", "rand.vo"]),
        ("run", {"file": "rterr.vo"}, ["run", "--json", "rterr.vo"]),
        ("run", {"file": "exit3.vo"}, ["run", "--json", "exit3.vo"]),
        ("run", {"file": "bad.vo"}, ["run", "--json", "bad.vo"]),
        ("run", {"file": "main.vo", "max_memory": 0}, ["run", "--json", "--max-memory", "0", "main.vo"]),
        ("run", {"file": "main.vo", "workers": 2}, ["run", "--json", "--workers", "2", "main.vo"]),
        ("run", {"file": "main.vo", "ca_files": ["nope.pem"]}, ["run", "--json", "--ca-file", "nope.pem", "main.vo"]),
        ("test", {"paths": ["main.vo"]}, ["test", "--json", "main.vo"]),
        ("test", {"paths": ["main.vo"], "filter": "tot"}, ["test", "--json", "--filter", "tot", "main.vo"]),
        ("test", {"paths": ["main.vo"], "mode": "list"}, ["test", "--json", "--list", "main.vo"]),
        ("test", {"paths": ["main.vo"], "mode": "coverage"}, ["test", "--json", "--coverage", "main.vo"]),
        ("test", {"paths": ["main.vo"], "mode": "mutate", "max_mutants": 5},
         ["test", "--json", "--mutate", "--max-mutants", "5", "main.vo"]),
        ("test", {"paths": ["main.vo"], "specs": ["S-1", "S-2"]},
         ["test", "--json", "--spec", "S-1", "--spec", "S-2", "main.vo"]),
        ("test", {"paths": ["main.vo"], "diff": DIFF, "seed": 3}, ["test", "--json", "--diff", difffile, "--seed", "3",
                                                                    "main.vo"]),
        ("fmt", {"paths": ["."], "check": True}, ["fmt", "--json", "--check", "."]),
        ("fmt", {"paths": ["ugly.vo"]}, ["fmt", "--json", "ugly.vo"]),
        ("effects", {"file": "main.vo"}, ["effects", "--json", "main.vo"]),
        ("effects", {"file": "main.vo", "update": True}, ["effects", "--json", "--update", "main.vo"]),
        ("effects", {"file": "main.vo", "update": False}, ["effects", "--json", "main.vo"]),
        ("explain", {"code": "E0301"}, ["explain", "--json", "E0301"]),
        ("explain", {"code": "nope"}, ["explain", "--json", "nope"]),
        ("doc", {}, ["doc", "--json"]),
        ("doc", {"name": "std.json"}, ["doc", "--json", "std.json"]),
        ("doc", {"entry": "main.vo", "name": "total"}, ["doc", "--json", "main.vo", "total"]),
        ("query", {"question": "callers", "entry": "main.vo", "target": "total"},
         ["query", "--json", "callers", "main.vo", "total"]),
        ("query", {"question": "refs", "entry": "main.vo", "target": "Item"}, ["query", "--json", "refs", "main.vo", "Item"]),
        ("query", {"question": "type-at", "entry": "main.vo", "target": "main.vo:7:9"},
         ["query", "--json", "type-at", "main.vo", "main.vo:7:9"]),
        ("query", {"question": "specs", "entry": "main.vo"}, ["query", "--json", "specs", "main.vo"]),
        ("query", {"question": "callers", "entry": "main.vo"}, ["query", "--json", "callers", "main.vo"]),
        ("impact", {"entry": "main.vo", "diff": DIFF}, ["impact", "--json", "main.vo", "--diff", difffile]),
        ("impact", {"entry": "main.vo", "diff": "not a diff"}, ["impact", "--json", "main.vo", "--diff", "-"],
         b"not a diff"),
        ("edit", {"edit": "rename", "entry": "main.vo", "target": "total", "change": "sum_of", "dry_run": True},
         ["edit", "--json", "--dry-run", "rename", "main.vo", "total", "sum_of"]),
        ("edit", {"edit": "rename", "entry": "main.vo", "target": "total", "change": "sum_of"},
         ["edit", "--json", "rename", "main.vo", "total", "sum_of"]),
        ("edit", {"edit": "add-field", "entry": "main.vo", "target": "Item", "change": "count: int = 1"},
         ["edit", "--json", "add-field", "main.vo", "Item", "count: int = 1"]),
        ("check", {"paths": ["main.vo"]}, ["check", "--json", "main.vo"]),
    ]
    for case in cases:
        tool, arguments, words = case[:3]
        label = f"{tool} {json.dumps(arguments)[:70]}"
        got = s.call(tool, arguments)
        out, err, code = direct(words, db, case[3] if len(case) > 3 else b"")
        if isinstance(got, dict):
            fail(label, f"protocol error {got}")
            continue
        blocks, exit_line, is_error = got
        want = [b.decode().replace(db + "/", da + "/") for b in (out, err) if b]  # (`edit` names vo.effects in full)
        ms = lambda texts: [re.sub(r'"ms":\d+', '"ms":0', t) for t in texts]  # a test's measured time
        if ms(blocks) != ms(want) or exit_line != {"v": 1, "kind": "exit", "status": code}:
            fail(label, f"differs from `vo {' '.join(words)}`\n  tool:   {blocks!r} {exit_line}\n"
                        f"  direct: {want!r} status {code}")
        if is_error != (code in (64, 125)):
            fail(label, f"isError {is_error} with status {code}")
    for f in sorted(set(os.listdir(da)) | set(os.listdir(db))):  # the writing tools left the same files
        pa, pb = os.path.join(da, f), os.path.join(db, f)
        if not (os.path.exists(pa) and os.path.exists(pb)) or open(pa, "rb").read() != open(pb, "rb").read():
            fail("files after the writing tools", f"{f} differs between the tool's and the command's directory")
    if "sum_of" not in open(os.path.join(da, "main.vo")).read() or "count" not in open(os.path.join(da, "main.vo")).read():
        fail("edit", "the edits did not change main.vo")
    if s.close() != 0:
        fail("equivalence", "exit status is not 0")


def validation(work):
    d = project(work, "val")
    before = {f: open(os.path.join(d, f)).read() for f in os.listdir(d)}
    s = Server(d)
    cases = [  # tool, arguments, what the message must name
        ("nope", {}, "nope"),
        ("check", {"paths": ["main.vo"], "extra": 1}, "extra"),
        ("check", {"paths": "main.vo"}, "paths"),
        ("check", {"paths": []}, "paths"),
        ("check", {"paths": ["main.vo", 3]}, "paths"),
        ("check", {}, "paths"),
        ("check", {"paths": ["--update"]}, "paths"),
        ("fmt", {"paths": ["ugly.vo", "--check"]}, "paths"),
        ("run", {"file": "main.vo", "timeout": "5"}, "timeout"),
        ("run", {"file": "main.vo", "timeout": 1.5}, "timeout"),
        ("run", {"file": "main.vo", "mem_stats": 1}, "mem_stats"),
        ("run", {"args": ["x"]}, "file"),
        ("run", {"file": "--json"}, "file"),
        ("run", {"file": "main\u0000.vo"}, "file"),
        ("run", {"file": "main.vo", "stdin": 4}, "stdin"),
        ("query", {"question": "who", "entry": "main.vo"}, "question"),
        ("test", {"paths": ["main.vo"], "mode": "fast"}, "mode"),
        ("edit", {"edit": "rename", "entry": "main.vo", "target": "total"}, "change"),
        ("fmt", {"paths": ["ugly.vo"], "check": "yes"}, "check"),
    ]
    for tool, arguments, names in cases:
        got = s.call(tool, arguments)
        label = f"invalid {tool} {json.dumps(arguments)[:60]}"
        if not isinstance(got, dict) or got.get("code") != -32602 or f"`{names}`" not in got.get("message", ""):
            fail(label, f"expected -32602 naming `{names}`, got {got}")
    for params, label in (({"name": "check", "arguments": [1]}, "arguments a list"),
                          ({"name": "check"}, "arguments missing"), ({"arguments": {}}, "name missing"), (None, "no params")):
        e = s.req("tools/call", params).get("error", {})
        if e.get("code") != -32602:
            fail(f"invalid tools/call ({label})", f"expected -32602, got {e}")
    words = s.req("tools/call", {"name": "check", "arguments": {"paths": ["x.vo"] * 300}}).get("error", {})
    if words.get("code") != -32602:
        fail("300 operands", f"expected -32602, got {words}")
    if {f: open(os.path.join(d, f)).read() for f in os.listdir(d)} != before:
        fail("validation", "a refused call changed a file")
    s.close()


def isolation(work):
    d = project(work, "iso")
    s = Server(d)
    first = s.call("run", {"file": "rand.vo", "seed": 5, "clock_ms": 9})

    def expect(label, got, status, code=None, where=1):
        if isinstance(got, dict):
            return fail(label, f"protocol error {got}")
        blocks, exit_line, is_error = got
        if exit_line.get("status") != status or is_error:
            fail(label, f"status {exit_line}, isError {is_error}; expected {status}")
        if code and not any(code in b for b in blocks):
            fail(label, f"no {code} in {blocks!r}"[:300])

    expect("timeout", s.call("run", {"file": "loop.vo", "timeout": 1}), 124, "R0009")
    expect("memory limit", s.call("run", {"file": "alloc.vo", "max_memory": 8}), 70, "R0006")
    expect("exit(3)", s.call("run", {"file": "exit3.vo"}), 3)
    expect("runtime error", s.call("run", {"file": "rterr.vo"}), 70, "R0")
    got = s.call("check", {"paths": ["exit3.vo"]})
    out, err, code = direct(["check", "--json", "exit3.vo"], d)
    if got != ([out.decode()], {"v": 1, "kind": "exit", "status": 0}, False):
        fail("check after failures", f"{got}")
    for arguments, stream in (({"file": "flood.vo"}, "stdout"), ({"file": "flood.vo", "args": ["e"]}, "stderr")):
        blocks, exit_line, _ = s.call("run", arguments)
        if len(blocks) != 1 or len(blocks[0]) != 1 << 20 or exit_line != {
                "v": 1, "kind": "exit", "status": 0, f"{stream}_omitted": 2 << 20}:
            fail(f"3 MiB on {stream}", f"{len(blocks)} blocks of {[len(b) for b in blocks]} bytes, {exit_line}")
    blocks, exit_line, _ = s.call("run", {"file": "raw.vo"})
    if blocks != ["h��i"] or exit_line.get("replaced") != 2:
        fail("bytes that are not UTF-8", f"{blocks!r} {exit_line}")
    if s.call("run", {"file": "rand.vo", "seed": 5, "clock_ms": 9}) != first:
        fail("the same call later in the session", "a different result than the first time")
    # two calls sent together are answered in order; then the end of input: both are answered
    a = s.ask("tools/call", {"name": "run", "arguments": {"file": "loop.vo", "timeout": 1}})
    b = s.ask("tools/call", {"name": "explain", "arguments": {"code": "E0301"}})
    s.p.stdin.close()
    got = [m and m.get("id") for m in (s.line(), s.line())]
    if got != [a, b] or s.p.wait(10 * SLOW) != 0:
        fail("two calls, then end of input", f"answers {got}, expected {[a, b]}, exit 0")
    s.p.stdout.close()

    # cancellation
    s = Server(d)
    s.ask("tools/call", {"name": "run", "arguments": {"file": "exit3.vo"}})  # queued work is not lost
    s.line()
    running = s.ask("tools/call", {"name": "run", "arguments": {"file": "loop.vo", "timeout": 0}})
    time.sleep(0.3 * SLOW)
    s.note("notifications/cancelled", {"requestId": running + 999})
    ping = s.ask("ping")
    time.sleep(0.3 * SLOW)
    if not others_in(d, s.p.pid):
        fail("cancel of another id", "the call was stopped")
    s.note("notifications/cancelled", {"requestId": running, "reason": "test"})
    msg = s.line(5)
    if not msg or msg.get("id") != ping:
        fail("cancellation", f"expected the ping's answer next, got {msg}")
    if not gone(d, s.p.pid):
        fail("cancellation", "the cancelled program is still running")
    if s.close() != 0:
        fail("cancellation", "exit status is not 0")

    # the host stops reading; the server is killed
    for how in ("stdout closed", "server killed"):
        s = Server(d)
        s.ask("tools/call", {"name": "run", "arguments": {"file": "loop.vo", "timeout": 0}})
        time.sleep(0.3 * SLOW)
        if not others_in(d, s.p.pid):
            fail(how, "the program did not start")
        if how == "stdout closed":
            s.p.stdout.close()
            try:
                if s.p.wait(3 * SLOW) != 0:
                    fail(how, "exit status is not 0")
            except subprocess.TimeoutExpired:
                fail(how, "the server did not exit")
                s.p.kill()
        else:
            s.p.send_signal(signal.SIGKILL)
            s.p.wait()
            s.p.stdout.close()
        if not gone(d, s.p.pid):
            fail(how, "the program is still running")
            for pid in others_in(d, s.p.pid):
                os.kill(pid, signal.SIGKILL)
        s.p.stdin.close()


def resources(work):
    d = project(work, "res")
    s = Server(d)
    r = s.req("resources/list")["result"]["resources"]
    if r != [{"uri": "vo://reference", "name": "reference", "title": "vo language reference",
              "description": "The whole language and toolchain in one document; read it before writing vo.",
              "mimeType": "text/markdown"}]:
        fail("resources/list", f"{r}")
    c = s.req("resources/read", {"uri": "vo://reference"})["result"]["contents"]
    ref = direct(["reference"], d)[0].decode()
    if len(c) != 1 or c[0].get("text") != ref or c[0].get("mimeType") != "text/markdown" or c[0].get("uri") != "vo://reference":
        fail("resources/read", "the text is not what `vo reference` prints")
    for params, code in (({"uri": "vo://other"}, -32002), ({"uri": "file:///etc/passwd"}, -32002), ({}, -32602), (None, -32602)):
        e = s.req("resources/read", params).get("error", {})
        if e.get("code") != code:
            fail(f"resources/read {params}", f"expected {code}, got {e}")
    if s.req("resources/templates/list").get("result") != {"resourceTemplates": []}:
        fail("resources/templates/list", "not an empty list")
    s.close()


def malformed(work):
    d = project(work, "mal")
    s = Server(d, init=False)
    e = s.req("tools/list").get("error", {})
    if e.get("code") != -32600:
        fail("tools/list before initialize", f"expected -32600, got {e}")
    e = s.req("tools/call", {"name": "check", "arguments": {"paths": ["main.vo"]}}).get("error", {})
    if e.get("code") != -32600:
        fail("tools/call before initialize", f"expected -32600, got {e}")
    s.req("initialize", {"protocolVersion": "2025-06-18"})

    def after(label, data, code):
        """data gets one answer with this error code and id null (None: no answer); then a ping works."""
        s.raw(data + b"\n")
        if code is not None:
            m = s.line(20)
            if not m or m.get("id", 0) is not None or m.get("error", {}).get("code") != code:
                fail(label, f"expected error {code} with id null, got {str(m)[:200]}")
        ping = s.ask("ping")
        m = s.line(20)
        if not m or m.get("id") != ping:
            fail(label, f"the following ping got {str(m)[:200]}")

    after("not JSON", b"hello", -32700)
    after("cut JSON", b'{"jsonrpc":"2.0","id":1,"method":"ping"', -32700)
    after("text after the object", b'{"jsonrpc":"2.0","id":1,"method":"ping"} x', -32700)
    after("a raw control character", b'{"jsonrpc":"2.0","id":1,"method":"pi\x01ng"}', -32700)
    after("bytes that are not UTF-8", b'{"jsonrpc":"2.0","id":1,"method":"pi\xffng"}', -32700)
    after("nesting 100 deep", b"[" * 100 + b"]" * 100, -32700)
    after("an empty batch", b"[]", -32600)
    after("a batch", b'[{"jsonrpc":"2.0","id":1,"method":"ping"},{"jsonrpc":"2.0","id":2,"method":"ping"}]', -32600)
    after("a number", b"42", -32600)
    after("no method", b'{"jsonrpc":"2.0","id":1}', -32600)
    after("method not a string", b'{"jsonrpc":"2.0","id":1,"method":7}', -32600)
    after("id an object", b'{"jsonrpc":"2.0","id":{},"method":"ping"}', -32600)
    after("id null", b'{"jsonrpc":"2.0","id":null,"method":"ping"}', -32600)
    after("5 MiB line", b'{"jsonrpc":"2.0","id":1,"method":"ping","params":{"x":"' + b"a" * (5 << 20) + b'"}}', -32600)
    after("an empty line", b"", None)
    after("a line of spaces", b"   \t ", None)
    after("an unknown notification", b'{"jsonrpc":"2.0","method":"nope/nope"}', None)
    after("an answer from the host", b'{"jsonrpc":"2.0","id":5,"result":{}}', None)
    after("a cancellation of nothing", b'{"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":1}}', None)
    e = s.req("nope/nope").get("error", {})
    if e.get("code") != -32601 or "nope/nope" not in e.get("message", ""):
        fail("unknown method", f"expected -32601 naming it, got {e}")
    for raw_id in (b'"a\\"b\\u00e9"', b"7", b"-3", b'""', b"1.5"):
        s.raw(b'{"jsonrpc":"2.0","method":"ping","id":' + raw_id + b"}\n")
        m = s.line(20)
        if not m or m.get("id") != json.loads(raw_id) or m.get("result") != {}:
            fail(f"id {raw_id.decode()}", f"got {m}")
    # escapes in arguments: a lone surrogate and an escaped NUL give a valid answer
    for value in ("\\ud800", "\\u0000", "\\ud83d\\ude00.vo", "a\\/b.vo"):
        s.raw(b'{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"check","arguments":{"paths":["'
              + value.encode() + b'"]}}}\n')
        m = s.line(20)
        if not m or m.get("id") != 9:
            fail(f"argument {value}", f"got {str(m)[:200]}")
    big = "x" * (3 << 20)  # a large but allowed message
    got = s.call("run", {"file": "main.vo", "stdin": big})
    if isinstance(got, dict) or got[1].get("status") != 0 or got[1].get("stdout_omitted", 0) < (2 << 20):
        fail("3 MiB of stdin", f"{str(got)[:200]}")
    if s.close() != 0:
        fail("malformed", "exit status is not 0")


def fuzz(work, n):
    d = project(work, "fuzz")
    rnd = random.Random(1)
    seeds = [json.dumps(m).encode() for m in (
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}},
        {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
        {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "explain", "arguments": {"code": "E0301"}}},
        {"jsonrpc": "2.0", "id": "x", "method": "tools/call",
         "params": {"name": "test", "arguments": {"paths": ["exit3.vo"], "mode": "list", "tests": ["a"], "seed": 1}}},
        {"jsonrpc": "2.0", "id": 4, "method": "resources/read", "params": {"uri": "vo://reference"}},
        {"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 3}})]
    lines = [seeds[0]]
    for _ in range(n):
        b = bytearray(rnd.choice(seeds))
        for _ in range(rnd.randint(0, 3)):
            kind, at = rnd.randint(0, 3), rnd.randrange(len(b))
            if kind == 0:
                b[at] = rnd.randrange(256)
            elif kind == 1:
                del b[at:at + rnd.randint(1, 8)]
            elif kind == 2:
                b[at:at] = rnd.choice([b'"', b"\\", b"{", b"}", b"[", b"]", b",", b":", b"\\u", b"\\ud800", b"-", b"1e9"])
            else:
                b[at:at] = b[rnd.randrange(len(b)):][:rnd.randint(1, 20)]
        lines.append(bytes(b).replace(b"\n", b" "))
    lines.append(b'{"jsonrpc":"2.0","id":"last","method":"ping"}')
    out = subprocess.run([VO, "mcp"], cwd=d, input=b"\n".join(lines) + b"\n", capture_output=True, timeout=600 * SLOW)
    answers = [json.loads(line.decode("utf-8")) for line in out.stdout.splitlines()]
    ok = out.returncode == 0 and answers and answers[-1] == {"jsonrpc": "2.0", "id": "last", "result": {}}
    print(f"fuzz: {n} lines, {len(answers)} answers, exit {out.returncode}, {'ok' if ok else 'FAIL'}")
    return ok


def session(work, n):
    """SC-002: the server's resident size after n mixed calls against after the first ten."""
    d = project(work, "session")
    s = Server(d)
    mix = [("check", {"paths": ["exit3.vo"]}), ("check", {"paths": ["bad.vo"]}), ("run", {"file": "rterr.vo"}),
           ("run", {"file": "main.vo", "stdin": "x" * 5000}), ("run", {"file": "alloc.vo", "max_memory": 8}),
           ("explain", {"code": "E0301"}), ("test", {"paths": ["main.vo"]}), ("run", {"file": "flood.vo"}),
           ("doc", {"name": "std.json"}), ("query", {"question": "refs", "entry": "main.vo", "target": "Item"})]

    def rss():
        pid = s.p.pid
        return next(int(line.split()[1]) for line in open(f"/proc/{pid}/status") if line.startswith("VmRSS"))

    early = 0
    for i in range(n):
        tool, arguments = mix[i % len(mix)] if i % 100 != 99 else ("run", {"file": "loop.vo", "timeout": 1})
        got = s.call(tool, arguments)
        if isinstance(got, dict):
            print(f"FAIL call {i}: {got}")
            return False
        if i == 9:
            early = rss()
    late = rss()
    s.req("resources/read", {"uri": "vo://reference"})
    ok = s.close() == 0 and late <= early
    print(f"SC-002: {n} calls answered; resident size {early} KiB after 10 calls, {late} KiB after {n}: "
          f"{'ok' if ok else 'FAIL'}")
    return ok


def timing(work):
    """SC-003: a call against the same command run directly; paired, alternating, median difference."""
    d = project(work, "time")
    s = Server(d)
    diffs = {}
    for tool, arguments, words in (("check", {"paths": ["main.vo"]}, ["check", "--json", "main.vo"]),
                                   ("run", {"file": "exit3.vo"}, ["run", "--json", "exit3.vo"]),
                                   ("explain", {"code": "E0301"}, ["explain", "--json", "E0301"])):
        a, b = [], []
        for r in range(41):
            for which in ((0, 1) if r % 2 == 0 else (1, 0)):
                t = time.perf_counter()
                if which == 0:
                    s.call(tool, arguments)
                else:
                    subprocess.run([os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))] + words, cwd=d,
                                   capture_output=True)
                (a if which == 0 else b).append(time.perf_counter() - t)
        diffs[tool] = statistics.median(x - y for x, y in zip(a, b)) * 1000
        print(f"SC-003 {tool}: tool {statistics.median(a) * 1000:.2f} ms, direct {statistics.median(b) * 1000:.2f} ms, "
              f"median difference {diffs[tool]:+.2f} ms")
    s.close()
    return max(diffs.values()) <= 5


def main():
    work = tempfile.mkdtemp(prefix="vo-mcp-")
    try:
        for flag, run in (("--session", session), ("--fuzz", fuzz)):
            if flag in sys.argv:
                sys.exit(0 if run(work, int(sys.argv[sys.argv.index(flag) + 1])) else 1)
        if "--time" in sys.argv:
            sys.exit(0 if timing(work) else 1)
        groups = (protocol_basics, tool_list, equivalence, validation, isolation, resources, malformed)
        for group in groups:
            try:
                group(work)
            except Exception as e:  # a hang or a line that is not JSON: the group ends, the rest still run
                fail(group.__name__, f"{type(e).__name__}: {e}")
        ok = fuzz(work, 300)
        if not ok:
            fails.append("fuzz")
        print(f"mcp: {len(groups)} groups, {len(fails)} failures")
        sys.exit(1 if fails else 0)
    finally:
        shutil.rmtree(work, ignore_errors=True)


main()
