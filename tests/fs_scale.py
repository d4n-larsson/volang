#!/usr/bin/env python3
"""Spec 017 SC-004: a big directory lists quickly, programs start cheaply, and nothing leaks.

  fs_scale.py [--entries N] [--runs M]   (defaults 100000 and 1000)

SC-001: a program walks a tree of 1,000 files in 50 directories, reads each, and writes a summary;
20 runs give byte-identical output and the same summary file. SC-004: lists a directory of N empty
files (limit 1 s), runs a trivial program M times (limit 5 s), then checks that no child process is
left and that a waiting vo program has the same open file descriptors before and after 20 rounds of
listings and runs.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "bin", "vo"))

LIST = """import std.fs;

fn main() -> result[void] uses console, file.read("big/") {
    let entries = try fs.list_dir("big");
    print(str(len(entries)) + " " + entries[0].name + " " + entries[len(entries) - 1].name);
    return ok();
}
"""

WALK = """import std.fs;

fn main() -> result[void] uses console, file.read("tree/"), file.write("out/") {
    var total = 0;
    var lines = 0;
    let files = try fs.walk("tree");
    for f in files {
        let text = try read_file(f);
        set total = total + len(text);
        set lines = lines + len(split(text, "\\n")) - 1;
    }
    try fs.make_dir("out");
    let summary = str(len(files)) + " files, " + str(lines) + " lines, " + str(total) + " bytes, first " + files[0] + ", last " + files[len(files) - 1];
    try write_file("out/summary.txt", summary + "\\n");
    print(summary);
    return ok();
}
"""

RUNS = """import std.process;

fn main() -> result[void] uses console, process("true") {
    var i = 0;
    var failed = 0;
    while i < COUNT {
        let out = try process.run("true", new list[str]);
        if out.code != 0 {
            set failed = failed + 1;
        }
        set i = i + 1;
    }
    print(str(COUNT) + " runs, " + str(failed) + " failed");
    return ok();
}
"""

LEAK = """import std.fs;
import std.process;

fn wait() uses console, input {
    print("waiting");
    match read_line() {
        case ok(_) {
        }
        case err(_) {
        }
    }
}

fn main() -> result[void] uses console, input, file.read("big/"), process("true") {
    wait();
    var round = 0;
    while round < 20 {
        let _ = try fs.list_dir("big");
        let _ = try process.run("true", new list[str]);
        set round = round + 1;
    }
    wait();
    return ok();
}
"""


def arg(name, default):
    return int(sys.argv[sys.argv.index(name) + 1]) if name in sys.argv else default


def children(d):
    n = 0
    for pid in os.listdir("/proc"):
        if pid.isdigit():
            try:
                n += os.path.realpath(os.readlink(f"/proc/{pid}/cwd")) == os.path.realpath(d)
            except OSError:
                pass
    return n


def main():
    n, m = arg("--entries", 100000), arg("--runs", 1000)
    bad = 0
    d = tempfile.mkdtemp()
    try:
        for i in range(1000):
            sub = os.path.join(d, "tree", f"d{i % 50:02d}", f"e{i % 7}")
            os.makedirs(sub, exist_ok=True)
            open(os.path.join(sub, f"f{i:04d}.txt"), "w").write("line\n" * (i % 5 + 1))
        open(os.path.join(d, "walk.vo"), "w").write(WALK)
        outs = set()
        start = time.monotonic()
        for _ in range(20):
            r = subprocess.run([VO, "run", "walk.vo"], cwd=d, capture_output=True, text=True)
            outs.add((r.returncode, r.stdout, r.stderr, open(os.path.join(d, "out", "summary.txt")).read()
                      if os.path.exists(os.path.join(d, "out", "summary.txt")) else None))
        dt = time.monotonic() - start
        ok = len(outs) == 1 and next(iter(outs))[0] == 0 and next(iter(outs))[1].startswith("1000 files")
        bad += not ok
        print(f"fs_scale: walking 1,000 files, 20 runs: {'identical' if ok else 'DIFFERENT or failed: ' + repr(outs)[:300]}"
              f" ({dt / 20:.2f} s per run): {next(iter(outs))[1].strip()}")
        big = os.path.join(d, "big")
        os.mkdir(big)
        for i in range(n):
            open(os.path.join(big, f"f{i:06d}"), "w").close()
        open(os.path.join(d, "list.vo"), "w").write(LIST)
        start = time.monotonic()
        r = subprocess.run([VO, "run", "list.vo"], cwd=d, capture_output=True, text=True)
        dt = time.monotonic() - start
        ok = r.returncode == 0 and r.stdout == f"{n} f000000 f{n - 1:06d}\n" and dt < 1.0
        bad += not ok
        print(f"fs_scale: listing {n} entries: {dt:.2f} s (limit 1 s)"
              f"{'' if ok else f' FAILED: exit {r.returncode} {r.stdout!r} {r.stderr[-300:]!r}'}")
        open(os.path.join(d, "runs.vo"), "w").write(RUNS.replace("COUNT", str(m)))
        start = time.monotonic()
        r = subprocess.run([VO, "run", "--timeout", "60", "runs.vo"], cwd=d, capture_output=True, text=True)
        dt = time.monotonic() - start
        ok = r.returncode == 0 and r.stdout == f"{m} runs, 0 failed\n" and dt < 5.0
        bad += not ok
        print(f"fs_scale: {m} runs of `true`: {dt:.2f} s, {dt / m * 1000:.2f} ms per run (limit 5 s)"
              f"{'' if ok else f' FAILED: exit {r.returncode} {r.stdout!r} {r.stderr[-300:]!r}'}")
        left = children(d)
        bad += left != 0
        print(f"fs_scale: {left} processes left in the directory")
        open(os.path.join(d, "leak.vo"), "w").write(LEAK)
        proc = subprocess.Popen([VO, "run", "--timeout", "0", "leak.vo"], cwd=d, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, text=True)
        proc.stdout.readline()
        before = len(os.listdir(f"/proc/{proc.pid}/fd"))
        proc.stdin.write("go\n")
        proc.stdin.flush()
        proc.stdout.readline()
        after = len(os.listdir(f"/proc/{proc.pid}/fd"))
        proc.stdin.write("done\n")
        proc.stdin.flush()
        proc.wait(timeout=60)
        ok = before == after and proc.returncode == 0
        bad += not ok
        print(f"fs_scale: 20 rounds of a listing and a run: {before} open fds before, {after} after "
              f"({'no leak' if ok else 'LEAK or failure'})")
    finally:
        shutil.rmtree(d)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
