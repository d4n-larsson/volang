#!/usr/bin/env python3
"""Spec 016 SC-006: many tasks complete within the default limits, and no file descriptor leaks.

  tasks_scale.py [--tasks N]   (default 10000)

Runs N trivial tasks at limit 10 and N/10 at limit 64 (default --max-memory, --timeout 120), checks the
results, and prints the time per task (the cost of a fork, three pipes and a result). Then a program
runs 20 calls of 500 tasks and waits on stdin: its open file descriptors must be the same as before
the calls.
"""
import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))

MANY = """fn work(i: int) -> int {
    return i * 2 + 1;
}

fn main() uses console {
    var xs = new list[int];
    var i = 0;
    while i < COUNT {
        push(xs, i);
        set i = i + 1;
    }
    let ys = concurrent_map(xs, LIMIT, work);
    var total = 0;
    for y in ys {
        set total = total + y;
    }
    print(str(len(ys)) + " " + str(total));
}
"""

FDS = """fn work(i: int) -> int {
    return i + 1;
}

fn wait() uses console, input {
    print("waiting");
    match read_line() {
        case ok(_) {
        }
        case err(_) {
        }
    }
}

fn main() uses console, input {
    var xs = new list[int];
    var i = 0;
    while i < 500 {
        push(xs, i);
        set i = i + 1;
    }
    wait();
    var round = 0;
    while round < 20 {
        let _ = concurrent_map(xs, 64, work);
        set round = round + 1;
    }
    wait();
}
"""


def fd_count(pid):
    return len(os.listdir(f"/proc/{pid}/fd"))


def main():
    n = int(sys.argv[sys.argv.index("--tasks") + 1]) if "--tasks" in sys.argv else 10000
    bad = 0
    with tempfile.TemporaryDirectory() as d:
        for count, limit in ((n, 10), (n // 10, 64)):
            path = os.path.join(d, "many.vo")
            open(path, "w").write(MANY.replace("COUNT", str(count)).replace("LIMIT", str(limit)))
            start = time.monotonic()
            r = subprocess.run([VO, "run", "--timeout", "120", path], capture_output=True, text=True)
            dt = time.monotonic() - start
            want = f"{count} {count * count}\n"
            ok = r.returncode == 0 and r.stdout == want
            bad += not ok
            print(f"tasks_scale: {count} tasks at limit {limit}: {dt:.2f} s, {dt / count * 1000:.3f} ms per task"
                  f"{'' if ok else f' FAILED: exit {r.returncode} {r.stdout!r} {r.stderr[-300:]!r}'}")
        path = os.path.join(d, "fds.vo")
        open(path, "w").write(FDS)
        proc = subprocess.Popen([VO, "run", "--timeout", "0", path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        proc.stdout.readline()
        pid = proc.pid  # bin/vo execs the binary (or qemu running it)
        before = fd_count(pid)
        proc.stdin.write("go\n")
        proc.stdin.flush()
        proc.stdout.readline()
        after = fd_count(pid)
        proc.stdin.write("done\n")
        proc.stdin.flush()
        proc.wait(timeout=60)
        ok = before == after and proc.returncode == 0
        bad += not ok
        print(f"tasks_scale: 20 calls of 500 tasks: {before} open fds before, {after} after "
              f"({'no leak' if ok else 'LEAK or failure'})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
