#!/usr/bin/env python3
"""Spec 040: streaming at its real sizes. The small behaviours are step cases (tests/cases/fs_steps/stream,
http/stream, process_steps/stream); this script covers what needs large inputs, time or a peer.

  stream.py                 every group (part of `make test`)
  stream.py --group NAME    one group: files, http, children, sweep, replay

VO_STREAM=small divides the large sizes by 20 (for the other architectures under qemu, where the time
bounds are not checked either).
"""
import hashlib
import os
import re
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tests"))
from tcp import CA, Peer, VO  # noqa: E402
from tcp_server import PATTERN  # noqa: E402

SMALL = os.environ.get("VO_STREAM") == "small"
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
BIG = 15 if SMALL else 300   # MiB of lines
DOWN = 5 if SMALL else 100   # MiB downloaded
FLOOD = 10 if SMALL else 200  # MiB printed by a child
failures = []
cases = 0

SUM = """import std.fs.each_line;

record Total {
    sum: int;
    lines: int;
}

fn main() uses console, file.read("./") {
    let total = new Total { sum: 0, lines: 0 };
    let done = each_line("NAME", fn(line: str) -> result[bool] {
        set total.sum = total.sum + try parse_int(line);
        set total.lines = total.lines + 1;
        return ok(true);
    });
    match done {
        case ok() {
            print(str(total.lines) + " " + str(total.sum));
        }
        case err(m) {
            print("err " + m + " after " + str(total.lines));
        }
    }
}
"""

CHUNKS = """import std.fs.each_chunk;

record Total {
    count: int;
    edges: int;
    pieces: int;
}

fn main() uses console, file.read("./") {
    let total = new Total { count: 0, edges: 0, pieces: 0 };
    let done = each_chunk("NAME", 1048576, fn(piece: bytes) -> result[bool] {
        set total.count = total.count + len(piece);
        set total.edges = total.edges + piece[0] + piece[len(piece) - 1];
        set total.pieces = total.pieces + 1;
        return ok(true);
    });
    match done {
        case ok() {
            print(str(total.count) + " " + str(total.edges) + " " + str(total.pieces));
        }
        case err(m) {
            print("err " + m);
        }
    }
}
"""

DOWNLOAD = """import std.fs.append_bytes;
import std.http.Header;
import std.http.Response;
import std.http.response_header;
import std.http.send_stream;

record Seen {
    total: int;
    largest: int;
    pieces: int;
}

fn main() uses console, file.write("./"), net("127.0.0.1:PORT") {
    let seen = new Seen { total: 0, largest: 0, pieces: 0 };
    let r = send_stream("METHOD", "SCHEME://127.0.0.1:PORTPATH", [new Header { name: "X-Asked", value: "yes" }], "BODY", WAIT, fn(head: Response, piece: bytes) -> result[bool] {
        try append_bytes("out.bin", piece);
        set seen.total = seen.total + len(piece);
        set seen.largest = max(seen.largest, len(piece));
        set seen.pieces = seen.pieces + 1;
        return ok(seen.pieces < STOP);
    });
    match r {
        case ok(head) {
            var mark = "";
            match response_header(head, "x-test") {
                case some(v) {
                    set mark = v;
                }
                case none {
                }
            }
            print(str(head.status) + " " + mark + " " + str(seen.total) + " " + str(seen.largest <= 65536) + " " + str(seen.pieces));
        }
        case err(m) {
            print("err " + m + " after " + str(seen.total));
        }
    }
}
"""

FOLLOW = """import std.process.run_lines;

record Seen {
    n: int;
    worst: int;
    pid: str;
}

fn main() uses console, clock, process("python3") {
    let seen = new Seen { n: 0, worst: 0, pid: "" };
    let r = run_lines("python3", ["child.py", "MODE"], "", new list[str], fn(line: str) -> result[bool] {
        set seen.n = seen.n + 1;
        if seen.n == 1 {
            set seen.pid = line;
            return ok(true);
        }
        if "MODE" == "tick" or "MODE" == "forever" {
            set seen.worst = max(seen.worst, now_ms() - try parse_int(line));
        }
        ACTION
        return ok(seen.n < STOP);
    });
    match r {
        case ok(out) {
            print(seen.pid + " " + str(seen.n) + " " + str(out.code) + " " + str(seen.worst));
        }
        case err(m) {
            print(seen.pid + " err " + m);
        }
    }
}
"""

CHILD = """import os, sys, time
mode = sys.argv[1]
print(os.getpid(), flush=True)
if mode == "tick":
    for _ in range(10):
        print(int(time.time() * 1000), flush=True)
        time.sleep(0.1)
elif mode == "forever":
    while True:
        print(int(time.time() * 1000), flush=True)
        time.sleep(0.05)
elif mode == "flood":
    line = "x" * 1023 + "\\n"
    for _ in range(%d * 1024):
        sys.stdout.write(line)
"""

FAILING_TEST = """import std.process.run_lines;

test "a failed check inside the function" uses console, process("python3") {
    let r = run_lines("python3", ["child.py", "forever"], "", new list[str], fn(line: str) -> result[bool] {
        print(line);
        expect(len(line) > 100);
        return ok(true);
    });
    expect_eq(r == r, true);
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def vo(d, words, timeout=300):
    r = subprocess.run([VO] + words, cwd=d, capture_output=True, text=True, timeout=timeout * SLOW)
    return r.returncode, r.stdout.strip(), r.stderr


def run(d, src, name, extra=(), timeout=300):
    with open(os.path.join(d, name), "w") as f:
        f.write(src)
    return vo(d, ["run", "--timeout", str(120 * SLOW)] + list(extra) + [name], timeout)


def numbers(d, name, mib):
    """A file of about mib MiB of numbers, one per line: (lines, their sum)."""
    block = "\n".join(str(i) for i in range(1, 1000001)) + "\n"
    times = max(1, (mib << 20) // len(block))
    with open(os.path.join(d, name), "w") as f:
        for _ in range(times):
            f.write(block)
    return times * 1000000, times * 500000500000


def peak(err):
    m = re.search(r"peak live ([\d.]+) MiB", err)
    return float(m.group(1)) if m else -1.0


def alive(pid):
    try:
        os.kill(int(pid), 0)
        return True
    except (OSError, ValueError):
        return False


def gone(pid):
    for _ in range(40):
        if not alive(pid):
            return True
        time.sleep(0.05)
    return False


def group_files(d):
    lines, total = numbers(d, "big.txt", BIG)
    t = time.time()
    code, out, err = run(d, SUM.replace("NAME", "big.txt"), "sum.vo", ["--max-memory", "16", "--mem-stats"])
    took = time.time() - t
    case(f"files/{BIG} MiB summed line by line under 16 MiB (SC-001)", (code, out) == (0, f"{lines} {total}"),
         f"{code} {out} {err[-300:]}")
    if not SMALL and SLOW == 1:
        case("files/in under 10 s (SC-001)", took < 10, f"{took:.1f} s")
    big_peak = peak(err)
    numbers(d, "ten.txt", 10)
    code, out, err = run(d, SUM.replace("NAME", "ten.txt"), "sum.vo", ["--max-memory", "16", "--mem-stats"])
    case("files/peak memory does not grow with the file (SC-005)", code == 0 and abs(peak(err) - big_peak) <= 1.0,
         f"10 MiB: {peak(err)} MiB, {BIG} MiB: {big_peak} MiB")
    data = open(os.path.join(d, "big.txt"), "rb").read()
    edges = sum(data[at] + data[min(at + (1 << 20), len(data)) - 1] for at in range(0, len(data), 1 << 20))
    pieces = (len(data) + (1 << 20) - 1) >> 20
    code, out, err = run(d, CHUNKS.replace("NAME", "big.txt"), "chunks.vo", ["--max-memory", "16"])
    case("files/every byte once, in 1 MiB chunks", (code, out) == (0, f"{len(data)} {edges} {pieces}"), f"{code} {out} {err[-300:]}")
    del data
    with open(os.path.join(d, "long.txt"), "w") as f:
        f.write("1\n" + "7" * (17 << 20))
    code, out, err = run(d, SUM.replace("NAME", "long.txt"), "sum.vo", ["--max-memory", "64"])
    case("files/a 17 MiB line", (code, out) == (0, "err line 2: longer than 16 MiB after 1"), f"{code} {out} {err[-300:]}")
    return lines, total


def download(d, port, path, scheme="http", method="GET", body="", wait=5000, stop=1 << 40, extra=()):
    out_bin = os.path.join(d, "out.bin")
    if os.path.exists(out_bin):
        os.remove(out_bin)
    open(out_bin, "wb").close()
    src = DOWNLOAD
    for k, v in (("PORT", port), ("PATH", path), ("SCHEME", scheme), ("METHOD", method), ("BODY", body),
                 ("WAIT", wait * SLOW), ("STOP", stop)):
        src = src.replace(k, str(v))
    return run(d, src, "down.vo", ["--max-memory", "32"] + list(extra))


def digest(d):
    h = hashlib.sha256()
    with open(os.path.join(d, "out.bin"), "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def group_http(d):
    want = hashlib.sha256(PATTERN * DOWN).hexdigest()
    with Peer("http") as p:
        for framing in ("length", "chunked", "close"):
            code, out, err = download(d, p.port, f"/{framing}/{DOWN}")
            parts = out.split()
            ok = code == 0 and parts[:4] == ["200", "yes", str(DOWN << 20), "true"] and digest(d) == want
            case(f"http/{DOWN} MiB, {framing}, under 32 MiB (SC-002)", ok, f"{code} {out} {err[-300:]}")
        t = time.time()
        code, out, err = download(d, p.port, f"/length/{DOWN}", stop=1)
        case("http/ok(false) stops after one piece", code == 0 and out.endswith(" true 1") and time.time() - t < 3 * SLOW,
             f"{code} {out} in {time.time() - t:.1f} s")
        code, out, err = download(d, p.port, "/stall", wait=300)
        case("http/a stalled body: an error after the time limit, the first half delivered",
             (code, out) == (0, "err the time limit passed after 500"), f"{code} {out}")
        for path in ("/bad-hex", "/bad-long", "/bad-end", "/bad-length"):
            code, out, err = download(d, p.port, path)
            case(f"http{path} is a malformed response", code == 0 and out.startswith("err malformed response: "), f"{code} {out} {err[-200:]}")
        code, out, err = download(d, p.port, "/nothing")
        case("http/204: the function is never called", (code, out) == (0, "204  0 true 0"), f"{code} {out}")
        code, out, err = download(d, p.port, "/echo", method="PUT", body="payload")
        got = open(os.path.join(d, "out.bin"), "rb").read()
        ok = got.startswith(b"PUT /echo HTTP/1.1\r\n") and b"X-Asked: yes\r\n" in got and got.endswith(b"\r\n\r\npayload") \
            and b"Content-Length: 7\r\n" in got
        case("http/method, headers and body arrive as with send", code == 0 and ok, f"{code} {out} {got[:200]!r}")
        src = DOWNLOAD.replace('net("127.0.0.1:PORT")', 'net("127.0.0.1:1")')
        for k, v in (("PORT", p.port), ("PATH", "/nothing"), ("SCHEME", "http"), ("METHOD", "GET"), ("BODY", ""),
                     ("WAIT", 1000), ("STOP", 9)):
            src = src.replace(k, str(v))
        code, out, err = run(d, src, "scope.vo")
        case("http/a URL outside the net scope is R0010", code != 0 and "R0010" in err, f"{code} {err[:200]}")
    with Peer("http", "a.test") as p:
        code, out, err = download(d, p.port, f"/chunked/{DOWN}", scheme="https", wait=10000, extra=["--ca-file", CA])
        parts = out.split()
        ok = code == 0 and parts[:4] == ["200", "yes", str(DOWN << 20), "true"] and digest(d) == want
        case(f"http/{DOWN} MiB over TLS (SC-002)", ok, f"{code} {out} {err[-300:]}")
    with Peer("http", "expired") as p:
        code, out, err = download(d, p.port, "/nothing", scheme="https", extra=["--ca-file", CA])
        case("http/a bad certificate is refused as for get", code == 0 and "has expired" in out, f"{code} {out}")


def follow(d, mode, stop=1 << 40, action="", extra=(), name="follow.vo"):
    with open(os.path.join(d, "child.py"), "w") as f:
        f.write(CHILD % FLOOD)
    src = FOLLOW.replace("MODE", mode).replace("STOP", str(stop)).replace("ACTION", action)
    return run(d, src, name, list(extra))


def group_children(d):
    code, out, err = follow(d, "tick")
    parts = out.split()
    case("children/every line delivered, the exit status returned", code == 0 and parts[1:3] == ["11", "0"], f"{code} {out} {err[-300:]}")
    if SLOW == 1:
        case("children/each line within 200 ms of its printing (SC-003)", code == 0 and int(parts[3]) < 200, f"worst {out}")
    t = time.time()
    code, out, err = follow(d, "forever", stop=4)
    parts = out.split()
    took = time.time() - t
    case("children/ok(false) ends the child (SC-003)", code == 0 and parts[1:3] == ["4", "137"] and gone(parts[0])
         and took < 2 * SLOW, f"{code} {out} in {took:.1f} s")
    code, out, err = follow(d, "forever", action='if seen.n == 3 {\n            return err("enough");\n        }')
    parts = out.split()
    case("children/an error from the function ends the child", code == 0 and parts[1:] == ["err", "enough"] and gone(parts[0]),
         f"{code} {out}")
    code, out, err = follow(d, "flood", extra=["--max-memory", "16"])
    parts = out.split()
    case(f"children/{FLOOD} MiB of output under 16 MiB", code == 0 and parts[1:3] == [str(FLOOD * 1024 + 1), "0"], f"{code} {out} {err[-300:]}")


def group_sweep(d):
    code, out, err = follow(d, "forever", action="if seen.n == 3 {\n            print(seen.pid);\n            exit(3);\n        }")
    case("sweep/exit inside the function leaves no child", code == 3 and out.isdigit() and gone(out), f"{code} {out} {err[-200:]}")
    code, out, err = follow(d, "forever", action="if seen.n == 3 {\n            print(seen.pid);\n            let none = new list[int];\n            print(str(none[0]));\n        }")
    pid = out.split()[0] if out else ""
    case("sweep/a runtime error inside the function leaves no child", code != 0 and "R0003" in err and gone(pid), f"{code} {out} {err[-200:]}")
    with open(os.path.join(d, "t.vo"), "w") as f:
        f.write(FAILING_TEST)
    code, out, err = vo(d, ["test", "t.vo"])
    pids = re.findall(r"^\s*(\d+)\s*$", out + "\n" + err, re.M)
    case("sweep/a failing test leaves no child", code != 0 and pids and gone(pids[0]), f"{code} {out[:300]} {err[:200]}")


def replayed(d, name, record, cleanup, want=None):
    code, out, err = record()
    case(f"replay/{name}: the recorded run", code == 0 and (want is None or out == want), f"{code} {out} {err[-300:]}")
    cleanup()
    code2, out2, err2 = vo(d, ["replay", "r.jsonl"])
    case(f"replay/{name} (SC-004)", (code2, out2) == (code, out), f"{code2} {out2} {err2[-300:]}")


def group_replay(d):
    lines, total = numbers(d, "ten.txt", 10)
    replayed(d, "lines", lambda: run(d, SUM.replace("NAME", "ten.txt"), "sum.vo", ["--record", "r.jsonl"]),
             lambda: os.remove(os.path.join(d, "ten.txt")), f"{lines} {total}")
    with open(os.path.join(d, "sum.vo"), "w") as f:
        f.write(SUM.replace("NAME", "other.txt"))
    code, out, err = vo(d, ["replay", "r.jsonl"])
    case("replay/another path is R0014", code != 0 and "R0014" in err, f"{code} {out} {err[:200]}")
    numbers(d, "ten.txt", 10)
    replayed(d, "chunks", lambda: run(d, CHUNKS.replace("NAME", "ten.txt"), "chunks.vo", ["--record", "r.jsonl"]),
             lambda: os.remove(os.path.join(d, "ten.txt")))
    for framing in ("length", "chunked"):
        p = Peer("http")
        replayed(d, f"download ({framing})", lambda: download(d, p.port, f"/{framing}/5", extra=["--record", "r.jsonl"]), p.stop)
    replayed(d, "a followed child", lambda: follow(d, "tick", extra=["--clock-ms", "0", "--record", "r.jsonl"]),
             lambda: os.remove(os.path.join(d, "child.py")))
    replayed(d, "a stopped child", lambda: follow(d, "forever", stop=4, extra=["--clock-ms", "0", "--record", "r.jsonl"]),
             lambda: os.remove(os.path.join(d, "child.py")))
    src = FOLLOW.replace("MODE", "tick").replace("STOP", str(1 << 40)).replace("ACTION", "").replace('["child.py", "tick"]', '["other.py", "tick"]')
    with open(os.path.join(d, "follow.vo"), "w") as f:
        f.write(src)
    code, out, err = vo(d, ["replay", "r.jsonl"])
    case("replay/other arguments are R0014", code != 0 and "R0014" in err, f"{code} {out} {err[:200]}")
    mib = 5 if SMALL else 100
    lines, total = numbers(d, "big.txt", mib)
    code, out, err = run(d, SUM.replace("NAME", "big.txt"), "sum.vo", ["--max-memory", "16", "--record", "big.jsonl"])
    size = os.path.getsize(os.path.join(d, "big.jsonl")) >> 20 if os.path.exists(os.path.join(d, "big.jsonl")) else 0
    case(f"replay/recording {mib} MiB stays under 16 MiB of memory", (code, out) == (0, f"{lines} {total}") and size >= mib,
         f"{code} {out} recording {size} MiB {err[-200:]}")


GROUPS = {"files": group_files, "http": group_http, "children": group_children, "sweep": group_sweep, "replay": group_replay}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    base = os.path.join(ROOT, "build", "tmp")  # (not /tmp: a third of a gigabyte may not fit a tmpfs)
    os.makedirs(base, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=base) as d:
        for n in names:
            sub = os.path.join(d, n)
            os.mkdir(sub)
            GROUPS[n](sub)
    print(f"stream: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
