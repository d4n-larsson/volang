#!/usr/bin/env python3
"""Runs a Python solution while recording the effects it uses (benchmark v2 capability grading).

Usage: py_sandbox.py main.py (from the run directory; the log path is in VO_SANDBOX_LOG).
Recorded, one JSON object per line: file.read / file.write (opens of paths that are not the
interpreter's own or the solution's modules), env (variable reads), clock (time and datetime),
random, net (socket use) and process (subprocess, os.system, exec). Console and stdin are not
recorded: every capability task allows them.
ponytail: audit hooks miss effects done through C extensions or ctypes; seccomp or strace if that matters
"""
import json
import os
import runpy
import sys

LOG = os.environ.get("VO_SANDBOX_LOG")
CWD = os.path.realpath(os.getcwd())
PREFIXES = tuple(os.path.realpath(p) for p in {sys.prefix, sys.base_prefix, sys.exec_prefix})
_log = open(LOG, "a") if LOG else None
_in_log = [False]


def record(effect, target=None):
    if _log is None or _in_log[0]:
        return
    _in_log[0] = True
    try:
        _log.write(json.dumps({"effect": effect, "target": target}) + "\n")
        _log.flush()
    finally:
        _in_log[0] = False


def hook(event, args):
    if _in_log[0]:
        return
    if event == "open":
        path, mode = args[0], args[1]
        if not isinstance(path, (str, bytes)) or isinstance(path, int):
            return
        path = os.fsdecode(path)
        real = os.path.realpath(path)
        if real.startswith(PREFIXES) or real == os.path.realpath(LOG or ""):
            return
        if real.endswith((".py", ".pyc")) and real.startswith(CWD):
            return                                      # the solution's own modules
        mode = mode or "r"
        writing = isinstance(mode, str) and any(c in mode for c in "wax+")
        if isinstance(mode, int):
            writing = bool(mode & (os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC | os.O_APPEND))
        record("file.write" if writing else "file.read", real)
    elif event in ("os.remove", "os.rename", "os.mkdir", "os.rmdir", "shutil.rmtree", "shutil.move"):
        record("file.write", os.path.realpath(os.fsdecode(args[0])))
    elif event.startswith("socket."):
        record("net", str(args[1]) if len(args) > 1 else None)
    elif event in ("subprocess.Popen", "os.system", "os.exec", "os.posix_spawn", "os.spawn"):
        record("process", str(args[0])[:200])


import collections.abc


class RecordingEnviron(collections.abc.MutableMapping):
    """os.environ that records reads: a name, or `*` when all of it is listed or copied."""
    def __init__(self, data):
        self._d = dict(data)

    def __getitem__(self, k):
        record("env", k)
        return self._d[k]

    def __setitem__(self, k, v):
        self._d[k] = v

    def __delitem__(self, k):
        del self._d[k]

    def __iter__(self):
        record("env", "*")
        return iter(dict(self._d))

    def __len__(self):
        return len(self._d)

    def __contains__(self, k):
        record("env", k)
        return k in self._d

    def copy(self):
        record("env", "*")
        return dict(self._d)


def wrap(module, names, effect):
    for n in names:
        f = getattr(module, n, None)
        if callable(f):
            def make(f=f, n=n):
                def wrapper(*a, **k):
                    record(effect, None)
                    return f(*a, **k)
                return wrapper
            setattr(module, n, make())


def main():
    target = sys.argv[1]
    sys.argv = sys.argv[1:]
    sys.path.insert(0, CWD)
    import time
    import random
    import datetime
    wrap(time, ["time", "time_ns", "monotonic", "monotonic_ns", "perf_counter", "perf_counter_ns",
                "localtime", "gmtime", "ctime", "asctime"], "clock")
    wrap(random, ["random", "randint", "randrange", "choice", "choices", "shuffle", "sample",
                  "uniform", "getrandbits", "gauss", "seed", "triangular"], "random")
    wrap(os, ["urandom"], "random")                     # also secrets, uuid4, SystemRandom

    class RecordingRandom(random.Random):              # random.Random() instances
        def random(self):
            record("random")
            return super().random()

        def getrandbits(self, k):
            record("random")
            return super().getrandbits(k)
    random.Random = RecordingRandom
    strftime = time.strftime

    def recording_strftime(fmt, *t):                   # without a time it reads the clock
        if not t:
            record("clock")
        return strftime(fmt, *t)
    time.strftime = recording_strftime

    class DT(datetime.datetime):
        @classmethod
        def now(cls, tz=None):
            record("clock")
            return datetime.datetime.now(tz)

        @classmethod
        def today(cls):
            record("clock")
            return datetime.datetime.today()

        @classmethod
        def utcnow(cls):
            record("clock")
            return datetime.datetime.utcnow()
    datetime.datetime = DT
    os.environ = RecordingEnviron(os.environ)
    sys.addaudithook(hook)
    runpy.run_path(target, run_name="__main__")


if __name__ == "__main__":
    main()
