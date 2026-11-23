#!/usr/bin/env python3
"""Spec 047: workers under volume, ended in every way, under time limits, timed, and replayed.

  workers.py                 every group (part of `make test`)
  workers.py --group NAME    one group: exchange, leaks, deadline, speed, replay

exchange  64 workers exchange 10,000 messages each way with the program, none lost, repeated or out
          of order (SC-001); three workers with fixed delays arrive in that order; a worker forwards
          lines from a connection before it closes; the bounds: 100,000 messages told without
          receiving, `full`, `too_large`
leaks     a program with workers computing, sleeping and waiting on the network, ended by return,
          `exit`, a runtime error, --timeout, SIGTERM and SIGKILL, 200 times each: no process of any
          run is left one second later (SC-002)
deadline  100 calls that never return, given 100 ms: each a timeout within 300 ms; a stopped worker
          is gone within 200 ms whatever it does (SC-003, FR-007)
speed     a round trip of a small message, a start from a program holding 100 MiB, four workers
          against one process (SC-004); reported everywhere, checked on x86-64 outside CI
replay    100 recordings of a program whose workers race replay to their own output and status; a
          program that names its workers prints the same 1,000 times (SC-005)

Every run has --timeout and a subprocess timeout: a hang is a failure, not a stuck suite.
"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
EMULATED = any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64"))
SLOW = 20 if EMULATED else 1
MARK = "VO_WORKERS_TEST"

failures = []
cases = 0


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


PRELUDE = """
import std.workers.Arrival;
import std.workers.Link;
import std.workers.Wait;
import std.workers.Worker;
import std.workers.WorkerError;
import std.workers.emit;
import std.workers.next_message;
import std.workers.receive;
import std.workers.receive_first;
import std.workers.start_worker;
import std.workers.stop_worker;
import std.workers.tell;
import std.workers.with_deadline;
import std.time.sleep;

fn why(e: WorkerError) -> str {
    match e {
        case timeout {
            return "timeout";
        }
        case ended {
            return "ended";
        }
        case failed(text) {
            return "failed: " + text;
        }
        case full {
            return "full";
        }
        case too_large {
            return "too_large";
        }
        case too_many {
            return "too_many";
        }
    }
}

fn sent(r: result[void, WorkerError]) -> str {
    match r {
        case ok() {
            return "sent";
        }
        case err(e) {
            return why(e);
        }
    }
}

fn num(text: str) -> int {
    match parse_int(text) {
        case ok(n) {
            return n;
        }
        case err(_) {
            return 0;
        }
    }
}

fn text_of(r: result[str]) -> str {
    match r {
        case ok(text) {
            return text;
        }
        case err(_) {
            return "?";
        }
    }
}

fn int_of(r: result[int, WorkerError]) -> int {
    match r {
        case ok(n) {
            return n;
        }
        case err(_) {
            return -1;
        }
    }
}

fn give[In, Out](link: Link[In, Out], message: Out) {
    match emit(link, message) {
        case ok() {
        }
        case err(_) {
        }
    }
}

fn started[In, Out](r: result[Worker[In, Out], WorkerError]) -> Worker[In, Out] {
    match r {
        case ok(w) {
            return w;
        }
        case err(e) {
            todo("no worker: " + why(e));
        }
    }
}
"""


def run(d, source, args=(), flags=(), timeout=60, env=None, stdin=None):
    """Runs a program in directory d; (stdout, stderr, status)."""
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(PRELUDE + source)
    cmd = [VO, "run", "--timeout", str(timeout * SLOW), *flags, "main.vo", *(["--", *args] if args else [])]
    r = subprocess.run(cmd, cwd=d, capture_output=True, text=True, timeout=(timeout + 30) * SLOW,
                       env={**os.environ, **(env or {})}, input=stdin)
    return r.stdout, r.stderr, r.returncode


def marked(mark):
    """The pids of processes whose environment holds MARK=mark."""
    want = f"{MARK}={mark}".encode()
    found = []
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            with open(f"/proc/{pid}/environ", "rb") as f:
                if want in f.read().split(b"\0"):
                    found.append(int(pid))
        except OSError:
            pass
    return found


# ---- exchange ----

EXCHANGE = """
fn echo(link: Link[int, int]) {
    while true {
        match next_message(link, Wait.forever) {
            case ok(n) {
                give(link, n + 1);
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() uses console {
    let count = num(args()[0]);
    let each = num(args()[1]);
    let ws = new list[Worker[int, int]];
    for i in 0..count {
        push(ws, started(start_worker(echo)));
    }
    var good = 0;
    var bad = 0;
    # in rounds: every worker is told a block, then every worker's answers are received
    var at = 0;
    while at < each {
        for i, w in ws {
            for k in at..at + 100 {
                if tell(w, i * 1000000 + k) != ok() {
                    set bad = bad + 1;
                }
            }
        }
        for i, w in ws {
            for k in at..at + 100 {
                if receive(w, Wait.forever) == ok(i * 1000000 + k + 1) {
                    set good = good + 1;
                } else {
                    set bad = bad + 1;
                }
            }
        }
        set at = at + 100;
    }
    for w in ws {
        if receive(w, Wait.at_most(0)) != err(WorkerError.timeout) {
            set bad = bad + 1;
        }
    }
    print(str(good) + " " + str(bad));
}
"""

DELAYS = """
fn after(ms: int) -> fn(Link[int, int]) uses clock {
    return fn(link: Link[int, int]) {
        sleep(ms);
        give(link, ms);
    };
}

fn main() uses console, clock {
    let ws = [started(start_worker(after(250 * SCALE))), started(start_worker(after(50 * SCALE))), started(start_worker(after(150 * SCALE)))];
    for i in 0..6 {
        match receive_first(ws, Wait.forever) {
            case ok(a) {
                match a.message {
                    case ok(ms) {
                        print(str(a.index) + " " + str(ms / SCALE));
                    }
                    case err(e) {
                        print(str(a.index) + " " + why(e));
                    }
                }
            }
            case err(e) {
                print(why(e) + " " + str(i));
            }
        }
    }
    print(sent(tell(ws[0], 1)));
    match receive_first(ws, Wait.forever) {
        case ok(_) {
            print("something");
        }
        case err(e) {
            print(why(e));
        }
    }
}
"""

LINES = """
import std.tcp.connect;
import std.tcp.read_until;
import std.bytes.from_text;
import std.bytes.to_text;

fn forward(port: int) -> fn(Link[int, str]) uses net {
    return fn(link: Link[int, str]) {
        match connect("127.0.0.1", port, 5000) {
            case ok(c) {
                while true {
                    match read_until(c, from_text("\\n"), 60000) {
                        case ok(line) {
                            give(link, trim(text_of(to_text(line))));
                        }
                        case err(_) {
                            return;
                        }
                    }
                }
            }
            case err(_) {
                give(link, "no connection");
            }
        }
    };
}

fn main() uses console, net {
    let w = started(start_worker(forward(num(args()[0]))));
    for i in 0..3 {
        match receive(w, Wait.at_most(20000)) {
            case ok(line) {
                print(line);
            }
            case err(e) {
                print(why(e));
            }
        }
    }
}
"""

FLOOD = """
fn echo(link: Link[int, int]) {
    while true {
        match next_message(link, Wait.forever) {
            case ok(n) {
                give(link, n);
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() uses console {
    let n = num(args()[0]);
    let w = started(start_worker(echo));
    var bad = 0;
    for i in 0..n {
        if tell(w, i) != ok() {
            set bad = bad + 1;
        }
    }
    for i in 0..n {
        if receive(w, Wait.forever) != ok(i) {
            set bad = bad + 1;
        }
    }
    print(str(bad));
}
"""

FULL = """
fn pours(link: Link[str, str]) {
    var piece = "0123456789abcdef";
    while len(piece) < 1000000 {
        set piece = piece + piece;
    }
    while true {
        give(link, piece);
    }
}

fn main() uses console {
    var piece = "0123456789abcdef";
    while len(piece) < 100000 {
        set piece = piece + piece;
    }
    let w = started(start_worker(pours));
    var told = 0;
    var answer = "sent";
    while answer == "sent" and told < 100000 {
        set answer = sent(tell(w, piece));
        set told = told + 1;
    }
    print(answer);
    print(sent(tell(w, piece)));
}
"""

LARGE = """
fn big(n: int) -> str {
    var piece = "0123456789abcdef";
    while len(piece) < n {
        set piece = piece + piece;
    }
    return piece;
}

fn answers(link: Link[str, str]) {
    match next_message(link, Wait.forever) {
        case ok(text) {
            match emit(link, big(17000000)) {
                case ok() {
                    give(link, "sent");
                }
                case err(e) {
                    give(link, why(e) + " in the worker, after " + str(len(text)));
                }
            }
        }
        case err(_) {
        }
    }
}

fn main() uses console {
    let w = started(start_worker(answers));
    print(sent(tell(w, big(17000000))));
    print(sent(tell(w, big(8000000))));
    match receive(w, Wait.forever) {
        case ok(text) {
            print(text);
        }
        case err(e) {
            print(why(e));
        }
    }
}
"""


def line_server():
    """Accepts one connection, sends three lines 100 ms apart, then holds it open; (port, closed)."""
    srv = socket.socket()
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    state = {"held": None}

    def serve():
        conn, _ = srv.accept()
        for i in range(3):
            conn.sendall(f"event {i}\n".encode())
            time.sleep(0.1)
        state["held"] = conn  # never closed by the server: the program must not wait for that

    threading.Thread(target=serve, daemon=True).start()
    return srv.getsockname()[1], state


def group_exchange():
    with tempfile.TemporaryDirectory(dir=os.path.join(ROOT, "build")) as d:
        count, each = (64, 10000) if not EMULATED else (64, 200)
        out, err, code = run(d, EXCHANGE, [str(count), str(each)], timeout=300)
        case("exchange/volume", (out.strip(), code) == (f"{count * each} 0", 0), f"{out!r} {err[:300]!r} {code}")

        scale = 4 if EMULATED else 1
        out, err, code = run(d, DELAYS.replace("SCALE", str(scale)))
        want = "1 50\n1 ended\n2 150\n2 ended\n0 250\n0 ended\nended\nended\n"
        case("exchange/first-by-delay", (out, code) == (want, 0), f"{out!r} {err[:300]!r}")

        port, state = line_server()
        t = time.time()
        out, err, code = run(d, LINES, [str(port)])
        took = time.time() - t
        case("exchange/lines-before-close", (out, code) == ("event 0\nevent 1\nevent 2\n", 0), f"{out!r} {err[:300]!r}")
        case("exchange/lines-not-waiting-for-close", took < 15 * SLOW, f"{took:.1f} s")

        n = 100000 if not EMULATED else 5000
        out, err, code = run(d, FLOOD, [str(n)], timeout=300)
        case("exchange/told-without-receiving", (out.strip(), code) == ("0", 0), f"{out!r} {err[:300]!r} {code}")

        out, err, code = run(d, FULL, timeout=300)
        case("exchange/full", (out, code) == ("full\nended\n", 0), f"{out!r} {err[:300]!r} {code}")

        out, err, code = run(d, LARGE, timeout=300)
        case("exchange/too-large", (out, code) == ("too_large\nsent\ntoo_large in the worker, after 8388608\n", 0),
             f"{out!r} {err[:300]!r} {code}")


# ---- leaks ----

LEAKS = """
import std.tcp.connect;
import std.tcp.read;

fn computes(link: Link[int, int]) {
    var n = 0;
    while true {
        set n = n + 1;
    }
}

fn sleeps(link: Link[int, int]) uses clock {
    sleep(600000);
}

fn hangs(port: int) -> fn(Link[int, int]) uses net {
    return fn(link: Link[int, int]) {
        match connect("127.0.0.1", port, 5000) {
            case ok(c) {
                match read(c, 100, 600000) {
                    case ok(_) {
                    }
                    case err(_) {
                    }
                }
            }
            case err(_) {
            }
        }
    };
}

fn inner(n: int) -> int uses clock {
    sleep(600000);
    return n;
}

fn maps(link: Link[int, int]) uses clock {
    let r = concurrent_map([1, 2], 2, inner);
    give(link, r[0]);
}

fn main() uses console, clock, net {
    let how = args()[0];
    let a = started(start_worker(computes));
    let b = started(start_worker(sleeps));
    let c = started(start_worker(hangs(num(args()[1]))));
    let d = started(start_worker(maps));
    sleep(30);
    print("ready");
    if how == "return" {
        return;
    }
    if how == "exit" {
        exit(3);
    }
    if how == "error" {
        let zero = len(chars(""));
        print(str(1 / zero));
    }
    sleep(600000);
}
"""


def group_leaks():
    runs = 200 if not EMULATED else 20
    batch = 20 if not EMULATED else 4
    hold = socket.socket()  # accepts (in the backlog) and never answers
    hold.bind(("127.0.0.1", 0))
    hold.listen(4096)
    port = hold.getsockname()[1]
    ways = {"return": 0, "exit": 3, "error": 70, "timeout": 124, "SIGTERM": -signal.SIGTERM, "SIGKILL": -signal.SIGKILL}
    with tempfile.TemporaryDirectory(dir=os.path.join(ROOT, "build")) as d:
        with open(os.path.join(d, "main.vo"), "w") as f:
            f.write(PRELUDE + LEAKS)
        for way, status in ways.items():
            mark = f"{os.getpid()}-{way}"
            how = way if way in ("return", "exit", "error") else "wait"
            limit = "1" if way == "timeout" else str(120 * SLOW)
            wrong = []
            for start in range(0, runs, batch):
                procs = [subprocess.Popen([VO, "run", "--timeout", limit, "main.vo", "--", how, str(port)], cwd=d,
                                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
                                          env={**os.environ, MARK: mark})
                         for _ in range(min(batch, runs - start))]
                for p in procs:
                    first = p.stdout.readline()
                    if first != "ready\n":
                        wrong.append(f"no ready: {first!r}")
                    if way.startswith("SIG"):
                        p.send_signal(getattr(signal, way))
                for p in procs:
                    try:
                        code = p.wait(timeout=60 * SLOW)
                    except subprocess.TimeoutExpired:
                        p.kill()
                        code = "hung"
                    p.stdout.close()
                    # under qemu a signal ends the emulator, whose status is 128 + the signal
                    if code != status and not (EMULATED and status < 0 and code in (status, 128 - status)):
                        wrong.append(f"status {code}")
            case(f"leaks/{way}/status", not wrong, f"{len(wrong)} of {runs}: {wrong[:3]}")
            time.sleep(1)
            left = marked(mark)
            case(f"leaks/{way}/none-left", not left, f"{len(left)} processes left after {runs} runs")
            for pid in left:
                try:
                    os.kill(pid, signal.SIGKILL)
                except OSError:
                    pass
    hold.close()


# ---- deadline ----

DEADLINE = """
import std.tcp.connect;
import std.tcp.read;

fn spin() -> int {
    var n = 0;
    while true {
        set n = n + 1;
    }
    return n;
}

fn computes(link: Link[int, int]) {
    var n = 0;
    while true {
        set n = n + 1;
    }
}

fn sleeps(link: Link[int, int]) uses clock {
    sleep(600000);
}

fn hangs(port: int) -> fn(Link[int, int]) uses net {
    return fn(link: Link[int, int]) {
        match connect("127.0.0.1", port, 5000) {
            case ok(c) {
                match read(c, 100, 600000) {
                    case ok(_) {
                    }
                    case err(_) {
                    }
                }
            }
            case err(_) {
            }
        }
    };
}

fn stop_time(w: Worker[int, int]) -> int uses clock {
    sleep(50);
    let t = now_ms();
    stop_worker(w);
    return now_ms() - t;
}

fn main() uses console, clock, net {
    var timeouts = 0;
    var slowest = 0;
    for i in 0..100 {
        let t = now_ms();
        if with_deadline(100, spin) == err(WorkerError.timeout) {
            set timeouts = timeouts + 1;
        }
        set slowest = max(slowest, now_ms() - t);
    }
    print(str(timeouts) + " " + str(slowest));
    print(str(stop_time(started(start_worker(computes)))) + " " + str(stop_time(started(start_worker(sleeps)))) + " "
        + str(stop_time(started(start_worker(hangs(num(args()[0])))))));
    sleep(1200);
    print(str(len(args())));
}
"""


def group_deadline():
    hold = socket.socket()
    hold.bind(("127.0.0.1", 0))
    hold.listen(16)
    mark = f"{os.getpid()}-deadline"
    with tempfile.TemporaryDirectory(dir=os.path.join(ROOT, "build")) as d:
        with open(os.path.join(d, "main.vo"), "w") as f:
            f.write(PRELUDE + DEADLINE)
        p = subprocess.Popen([VO, "run", "--timeout", str(300 * SLOW), "main.vo", "--", str(hold.getsockname()[1])], cwd=d,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={**os.environ, MARK: mark})
        first = p.stdout.readline().split()
        second = p.stdout.readline().split()
        # the program now sleeps: only its own process (and bin/vo's shell, if any) may carry the mark
        time.sleep(0.5)
        own = marked(mark)
        out, err = p.communicate(timeout=120 * SLOW)
        case("deadline/100-timeouts", first[:1] == ["100"], f"{first} {err[:300]!r}")
        if len(first) == 2:
            print(f"deadline: the slowest of 100 calls with 100 ms took {first[1]} ms")
            if not EMULATED:
                case("deadline/within-300ms", int(first[1]) <= 300, f"{first[1]} ms")
        if len(second) == 3:
            print(f"stop: computing {second[0]} ms, sleeping {second[1]} ms, on the network {second[2]} ms")
            if not EMULATED:
                case("deadline/stop-within-200ms", max(map(int, second)) <= 200, str(second))
        else:
            case("deadline/stop", False, f"{second} {err[:300]!r}")
        case("deadline/processes-gone", len(own) <= 2, f"{len(own)} processes while the program sleeps")
    hold.close()


# ---- speed ----

SPEED = """
record Pair {
    a: int;
    b: int;
}

fn swaps(link: Link[Pair, Pair]) {
    while true {
        match next_message(link, Wait.forever) {
            case ok(p) {
                give(link, new Pair { a: p.b, b: p.a });
            }
            case err(_) {
                return;
            }
        }
    }
}

fn idle(link: Link[int, int]) {
}

record Range {
    lo: int;
    hi: int;
}

fn work(r: Range) -> int {
    var total = 0;
    for i in r.lo..r.hi {
        set total = (total + i * i) % 1000003;
    }
    return total;
}

fn sums(link: Link[Range, int]) {
    while true {
        match next_message(link, Wait.forever) {
            case ok(r) {
                give(link, work(r));
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() uses console, clock {
    let trips = num(args()[0]);
    let n = num(args()[1]);
    let w = started(start_worker(swaps));
    let t0 = now_ms();
    var good = 0;
    for i in 0..trips {
        match tell(w, new Pair { a: i, b: 1 }) {
            case ok() {
            }
            case err(_) {
            }
        }
        if receive(w, Wait.forever) == ok(new Pair { a: 1, b: i }) {
            set good = good + 1;
        }
    }
    print("trip " + str((now_ms() - t0) * 1000000 / trips) + " " + str(good));
    stop_worker(w);

    let held = new list[str];
    var piece = "0123456789abcdef";
    while len(piece) < 1000000 {
        set piece = piece + piece;
    }
    for i in 0..100 {
        push(held, piece + str(i));
    }
    var slowest = 0;
    for i in 0..10 {
        let t = now_ms();
        let s = started(start_worker(idle));
        set slowest = max(slowest, now_ms() - t);
        stop_worker(s);
    }
    print("start " + str(slowest) + " " + str(len(held)));

    let t1 = now_ms();
    var alone = 0;
    for k in 0..8 {
        set alone = (alone + work(new Range { lo: k * n, hi: (k + 1) * n })) % 1000003;
    }
    let one = now_ms() - t1;
    let pool = new list[Worker[Range, int]];
    for k in 0..4 {
        push(pool, started(start_worker(sums)));
    }
    let t2 = now_ms();
    var together = 0;
    for round in 0..2 {
        for k, p in pool {
            match tell(p, new Range { lo: (round * 4 + k) * n, hi: (round * 4 + k + 1) * n }) {
                case ok() {
                }
                case err(_) {
                }
            }
        }
        for p in pool {
            set together = (together + int_of(receive(p, Wait.forever))) % 1000003;
        }
    }
    let four = now_ms() - t2;
    print("pool " + str(one) + " " + str(four) + " " + str(alone == together));
}
"""


def group_speed():
    trips = 20000 if not EMULATED else 500
    n = 15000000 if not EMULATED else 50000
    with tempfile.TemporaryDirectory(dir=os.path.join(ROOT, "build")) as d:
        out, err, code = run(d, SPEED, [str(trips), str(n)], timeout=600)
        lines = dict((l.split()[0], l.split()[1:]) for l in out.splitlines() if l.strip())
        case("speed/ran", code == 0 and set(lines) == {"trip", "start", "pool"}, f"{out!r} {err[:300]!r} {code}")
        if set(lines) != {"trip", "start", "pool"}:
            return
        ns, good = int(lines["trip"][0]), int(lines["trip"][1])
        start = int(lines["start"][0])
        one, four, same = int(lines["pool"][0]), int(lines["pool"][1]), lines["pool"][2]
        cores = os.cpu_count() or 1
        print(f"speed: a round trip {ns / 1000:.1f} microseconds; a start holding 100 MiB {start} ms; "
              f"one process {one} ms, four workers {four} ms ({one / max(four, 1):.2f} times, {cores} cores)")
        case("speed/all-trips", good == trips, f"{good} of {trips}")
        case("speed/pool-same-result", same == "true", same)
        if EMULATED or os.environ.get("CI") or os.uname().machine != "x86_64":
            return
        case("speed/round-trip-under-50us", ns < 50000, f"{ns} ns")
        case("speed/start-under-10ms", start < 10, f"{start} ms")
        if cores >= 4:
            case("speed/four-workers-2.5x", one >= 2.5 * four, f"{one} ms alone, {four} ms with four")


# ---- replay ----

RACE = """
fn runner(id: int) -> fn(Link[int, int]) uses console, clock, random {
    return fn(link: Link[int, int]) {
        for i in 0..5 {
            sleep(random_int(0, 3));
            print("runner " + str(id) + " at " + str(i));
            give(link, id * 10 + i);
        }
        if id == 2 {
            let zero = len(chars(""));
            give(link, 1 / zero);
        }
    };
}

fn slow(link: Link[int, int]) uses console, clock {
    print("slow: started");
    sleep(600000);
}

fn main() uses console, clock, random {
    let ws = [started(start_worker(runner(1))), started(start_worker(runner(2))), started(start_worker(runner(3)))];
    let late = started(start_worker(slow));
    let unasked = started(start_worker(runner(2)));
    var done = false;
    while not done {
        match receive_first(ws, Wait.at_most(2)) {
            case ok(a) {
                match a.message {
                    case ok(n) {
                        print("got " + str(n) + " from " + str(a.index));
                    }
                    case err(e) {
                        print(str(a.index) + ": " + slice(why(e), 0, 6));
                    }
                }
            }
            case err(e) {
                print(why(e));
                set done = why(e) == "ended";
            }
        }
    }
    match receive(late, Wait.at_most(5)) {
        case ok(_) {
            print("late spoke");
        }
        case err(e) {
            print("late: " + why(e));
        }
    }
    stop_worker(late);
    sleep(random_int(0, 20));
}
"""

NAMED = """
fn talker(id: int) -> fn(Link[int, int]) uses console, clock, random {
    return fn(link: Link[int, int]) {
        for i in 0..4 {
            sleep(random_int(0, 2));
            print("talker " + str(id) + " says " + str(i));
            give(link, id * 10 + i);
        }
        print("talker " + str(id) + " is done");
    };
}

fn main() uses console, clock, random {
    let ws = [started(start_worker(talker(1))), started(start_worker(talker(2))), started(start_worker(talker(3)))];
    for round in 0..5 {
        for i, w in ws {
            match receive(w, Wait.forever) {
                case ok(n) {
                    print(str(i) + " " + str(n) + " " + sent(tell(w, n)));
                }
                case err(e) {
                    print(str(i) + " " + why(e) + " " + sent(tell(w, 0)));
                }
            }
        }
    }
}
"""


def group_replay():
    recordings = 100 if not EMULATED else 10
    same_runs = 1000 if not EMULATED else 20
    with tempfile.TemporaryDirectory(dir=os.path.join(ROOT, "build")) as d:
        with open(os.path.join(d, "main.vo"), "w") as f:
            f.write(PRELUDE + RACE)
        differ, outputs = [], set()
        for i in range(recordings):
            r = subprocess.run([VO, "run", "--timeout", str(60 * SLOW), "--record", "r.jsonl", "main.vo"], cwd=d,
                               capture_output=True, text=True, timeout=120 * SLOW)
            p = subprocess.run([VO, "replay", "r.jsonl"], cwd=d, capture_output=True, text=True, timeout=120 * SLOW)
            outputs.add(r.stdout)
            if (r.stdout, r.stderr, r.returncode) != (p.stdout, p.stderr, p.returncode) or r.returncode != 70:
                differ.append((i, r.returncode, p.returncode, r.stdout[-200:], p.stdout[-200:], p.stderr[-300:]))
        case("replay/own-output", not differ, f"{len(differ)} of {recordings}: {differ[:1]}")
        print(f"replay: {recordings} recordings of the racing program, {len(outputs)} different outputs")
        if not EMULATED:
            case("replay/the-race-is-real", len(outputs) > 1, "every run printed the same: the program does not race")

        with open(os.path.join(d, "main.vo"), "w") as f:
            f.write(PRELUDE + NAMED)
        seen = set()
        batch = 20 if not EMULATED else 4
        for start in range(0, same_runs, batch):
            procs = [subprocess.Popen([VO, "run", "--timeout", str(60 * SLOW), "main.vo"], cwd=d, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, text=True) for _ in range(min(batch, same_runs - start))]
            for p in procs:
                out, err = p.communicate(timeout=120 * SLOW)
                seen.add((out, err, p.returncode))
        one = next(iter(seen))
        case("replay/named-receives-same-output", len(seen) == 1 and one[2] == 0 and one[0].count("\n") == 30,
             f"{len(seen)} different results in {same_runs} runs; {one[0][-200:]!r} {one[1][-200:]!r} {one[2]}")


GROUPS = {"exchange": group_exchange, "leaks": group_leaks, "deadline": group_deadline, "speed": group_speed,
          "replay": group_replay}


def main():
    names = list(GROUPS)
    if "--group" in sys.argv:
        names = [sys.argv[sys.argv.index("--group") + 1]]
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    for name in names:
        GROUPS[name]()
    if failures:
        print(f"workers: {len(failures)} of {cases} checks failed")
        sys.exit(1)
    print(f"workers: {cases} checks passed ({', '.join(names)})")


if __name__ == "__main__":
    main()
