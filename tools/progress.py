#!/usr/bin/env python3
"""Progress and estimated time for the long make targets (test, gate, gc-check, ...).

  tools/progress.py run TARGET [MAKE_ARG...]   runs `make TARGET`, its output to build/progress/TARGET.log
  tools/progress.py watch LOG [TARGET]         follows a log another run is writing (TARGET: its timings)

A step is a line that starts with `== ` (`== gate: test`, `== fmt_corpus.py`, ...). The status line shows
the step, how long it and the whole run have taken, and an estimate of what is left: the durations the
same steps took in the last complete run of TARGET (build/progress/TARGET.times.json, written when a
`run` ends). The first run of a target has no estimate. FAIL lines and the last lines before a failure
are printed as they come; the full output is in the log.
"""
import json
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, "build", "progress")
STEP = re.compile(r"^== (.+)$")
TTY = sys.stderr.isatty()


def fmt(s):
    s = int(s)
    return f"{s // 3600}h{s % 3600 // 60:02d}m" if s >= 3600 else f"{s // 60}m{s % 60:02d}s"


class Progress:
    def __init__(self, target, start):
        self.target, self.start = target, start
        self.steps = []  # [key, name, started, ended]
        self.seen = {}
        self.history = {}
        path = os.path.join(DIR, f"{target}.times.json") if target else None
        if path and os.path.exists(path):
            self.history = json.load(open(path))
        self.last_draw = 0
        self.quiet = False  # catching up on a log that is already there: no drawing

    def line(self, text):
        m = STEP.match(text)
        if m:
            now = None if self.quiet else time.time()  # a step already in the log: its start is unknown
            if self.steps:
                self.steps[-1][3] = now
            name = m.group(1).strip()
            k = self.seen.get(name, 0) + 1
            self.seen[name] = k
            self.steps.append([f"{name}#{k}", name, now, None])
            self.draw(force=True)
        elif text.startswith("FAIL") or "*** " in text:
            self.clear()
            print(text, file=sys.stderr)
            self.draw(force=True)

    def remaining(self, now):
        """Seconds left by the last run's timings, or None without them."""
        if not self.history:
            return None
        left = 0.0
        done = {s[0] for s in self.steps}
        cur = self.steps[-1] if self.steps else None
        if cur:
            left += max(0.0, self.history.get(cur[0], 0) - (now - cur[2] if cur[2] else 0))
        for key, secs in self.history.items():
            if key not in done:
                left += secs
        return left

    def clear(self):
        if TTY:
            sys.stderr.write("\r\033[K")

    def draw(self, force=False):
        if self.quiet:
            return
        now = time.time()
        if not force and now - self.last_draw < (1 if TTY else 60):
            return
        self.last_draw = now
        cur = self.steps[-1] if self.steps else None
        total = len(self.history) or "?"
        parts = [f"[{len(self.steps)}/{total}]", cur[1][:50] if cur else "starting",
                 f"step {fmt(now - cur[2])}" if cur and cur[2] else "", f"total {fmt(now - self.start)}"]
        left = self.remaining(now)
        parts.append(f"about {fmt(left)} left" if left is not None else "no estimate yet (first run)")
        text = "  ".join(p for p in parts if p)
        if TTY:
            sys.stderr.write("\r\033[K" + text[: os.get_terminal_size(2).columns - 1])
        else:
            sys.stderr.write(text + "\n")
        sys.stderr.flush()

    def save(self):
        now = time.time()
        if self.steps and self.steps[-1][3] is None:
            self.steps[-1][3] = now
        os.makedirs(DIR, exist_ok=True)
        times = {s[0]: round(s[3] - s[2], 1) for s in self.steps}
        json.dump(times, open(os.path.join(DIR, f"{self.target}.times.json"), "w"), indent=1)

    def summary(self, status):
        self.clear()
        end = time.time()
        timed = [s for s in self.steps if s[2]]
        slow = sorted(timed, key=lambda s: -((s[3] or end) - s[2]))[:8]
        state = "ended" if status is None else "passed" if status == 0 else f"FAILED (exit {status})"
        print(f"{self.target or 'run'}: {state} "
              f"in {fmt(end - self.start)}, {len(self.steps)} steps; slowest:", file=sys.stderr)
        for s in slow:
            print(f"  {fmt((s[3] or end) - s[2]):>8}  {s[1]}", file=sys.stderr)


def run(target, args):
    os.makedirs(DIR, exist_ok=True)
    log = os.path.join(DIR, f"{target}.log")
    p = Progress(target, time.time())
    tail = []
    with open(log, "w") as out:
        proc = subprocess.Popen(["make", target] + args, cwd=ROOT, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1)
        print(f"make {target}: output in {os.path.relpath(log, ROOT)}", file=sys.stderr)
        for text in proc.stdout:
            out.write(text)
            out.flush()
            text = text.rstrip("\n")
            tail = (tail + [text])[-15:]
            p.line(text)
            p.draw()
        status = proc.wait()
    if status == 0:
        p.save()
    else:
        p.clear()
        print("\n".join(tail), file=sys.stderr)
    p.summary(status)
    return status


def birth(path):
    """The log's creation time where the file system keeps it, else now."""
    try:
        t = int(subprocess.run(["stat", "-c", "%W", path], capture_output=True, text=True).stdout)
        return t if t > 0 else time.time()
    except ValueError:
        return time.time()


def watch(log, target):
    p = Progress(target, birth(log))
    with open(log, errors="replace") as f:
        p.quiet = True  # what is there already (its steps' times are unknown), then follow
        for text in f:
            p.line(text.rstrip("\n"))
        p.quiet = False
        p.draw(force=True)
        while True:
            text = f.readline()
            if text:
                p.line(text.rstrip("\n"))
                continue
            if writer_gone(log):
                break
            p.draw()
            time.sleep(1)
    p.summary(None)


def writer_gone(log):
    """No process has the log open any more (the run ended)."""
    r = subprocess.run(["fuser", log], capture_output=True)
    return r.returncode != 0


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "run":
        return run(sys.argv[2], sys.argv[3:])
    if len(sys.argv) in (3, 4) and sys.argv[1] == "watch":
        try:
            watch(sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
        except KeyboardInterrupt:
            pass
        return 0
    print(__doc__, file=sys.stderr)
    return 64


if __name__ == "__main__":
    sys.exit(main())
