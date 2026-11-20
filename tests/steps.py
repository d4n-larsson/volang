#!/usr/bin/env python3
"""Step cases (spec 006): golden cases that need more than one command, or that produce files.

A step case is a folder under tests/cases/ that contains a file named `steps` (tests/run.sh skips
such folders). Everything in the folder except `expected/` is copied to a temporary directory, and
each line of `steps` runs there, in order:

  vo ARGS... [< FILE]          run bin/vo ($VO_BIN, natively or under qemu) with stdin from FILE (default: empty)
  env NAME=VALUE ... vo ARGS... the same, with extra environment variables (\\\\xNN in VALUE, written so, is that byte)
  unset NAME ... vo ARGS...     the same, without those variables
  app PATH ARGS... [< FILE]    (spec 034) runs the executable at PATH, built by `vo build`, with ARGS,
                                natively or under qemu like vo; reported like a `vo` step
  rm PATH | cp A B | write PATH TEXT (TEXT with \\n and \\xNN escapes, {bytes:N}) | chmod MODE PATH
  symlink TARGET LINK | mkdir PATH | mkfifo PATH (spec 013)
  helpers                       (spec 017) copies tests/process_helpers/* into ./bin/
  touch PATH                    (spec 017) an empty file; \\xNN in PATH is that byte (names that are not UTF-8)
  keep PATH...                  files the case produces or changes, compared with expected/PATH
  server http                   starts tests/http_server.py on a free port (spec 007): later lines may
                                use {port}, and the port number reads `{port}` in outputs and kept files
  server https NAME [OPTION...] (spec 019) starts tests/https_server.py with certificate NAME of
                                tests/tls/certs/ and its OPTIONs (--tls12-only, --tamper, --log FILE, ...)
                                on a free port; {port} as for `server http`
  no-trust-store vo ARGS...     (spec 019) runs vo where none of the machine's trust store paths exist
                                (a user and mount namespace with tmpfs over /etc/ssl and /etc/pki); where
                                namespaces are not permitted, prints `skipped: no user namespaces` instead
  stop-server                   stops it ({port} keeps its number: a refused connection)
  start vo ARGS...              starts bin/vo in the background (spec 011) with {vport} a free port, and
                                waits until it accepts connections (or exits); {vport} reads `{vport}`
  request METHOD PATH [BODY]    sends one HTTP/1.1 request to {vport}; the raw response goes to `out`
  request-raw TEXT              sends TEXT as is (\\r, \\n and \\xNN escapes; {bytes:N} is N bytes `a`)
  request-open NAME TEXT        (spec 016) sends TEXT like request-raw on a connection NAME that stays open,
                                unread, until read-open NAME (or the end of the case)
  read-open NAME                reads connection NAME until the server closes it; the response goes to `out`
  no-children                   (spec 016) `out` gets how many processes still run in the case's directory
                                (left-over tasks), after up to 2 s for them to end
  request-many N METHOD PATH    sends N requests at once; `out` gets their bodies sorted (numbers by value)
                                and whether every one was answered within 1 s
  wait MS                       lets MS milliseconds pass
  signal TERM|INT               signals the started vo and waits; its stdout, stderr and exit status go to `out`
  vo~ ARGS...                   like vo, but its stdout is left out of `out` (it may differ between
                                runs, e.g. with a clock seed); `same` then checks the last two were equal

No shell is involved; arguments are split on spaces ("quoted words" may hold spaces).
Afterwards every file in expected/ except `out` must exist with identical bytes, and expected/out
must equal, for each `vo` step: `$ LINE`, its stdout, `--- stderr`, its stderr, `--- exit N`.
Timings (`12ms)`, `"ms":12`) are masked as in run.sh. With VO_REPLAY_BIN set, `vo replay` steps run
that build instead (tests/cross_replay.py).

  steps.py [--accept] [--repeat N] [PATH...]
"""
import os
import re
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")  # runs $VO_BIN (default build/vo) natively or under qemu


def mask(b, port=None, vport=None):
    # (a whole number only: a random port must not be masked inside a hash or a longer number)
    if port:
        b = re.sub(rb"(?<![0-9a-fA-F])" + str(port).encode() + rb"(?![0-9a-fA-F])", b"{port}", b)
    if vport:
        b = re.sub(rb"(?<![0-9a-fA-F])" + str(vport).encode() + rb"(?![0-9a-fA-F])", b"{vport}", b)
    b = re.sub(rb'"ms":\d+', b'"ms":0', b)
    # spec 034: the size of a built executable follows the toolchain's, which differs by platform
    b = re.sub(rb"(?m)^(built .*, )\d+ bytes$", rb"\g<1>0 bytes", b)
    b = re.sub(rb'("kind":"build".*"bytes":)\d+', rb"\g<1>0", b)
    return re.sub(rb"([ (])\d+ms\)", rb"\g<1>0ms)", b)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_listening(port, proc):
    """Until something accepts on 127.0.0.1:port, or the process exits (up to 30 s)."""
    end = time.time() + 30
    while time.time() < end and proc.poll() is None:
        try:
            socket.create_connection(("127.0.0.1", port), timeout=1).close()
            return
        except OSError:
            time.sleep(0.05)


def raw_bytes(text):
    """request-raw's TEXT: \\r, \\n, \\xNN escapes and {bytes:N}."""
    text = re.sub(r"\{bytes:(\d+)\}", lambda m: "a" * int(m.group(1)), text)
    data = text.replace("\\r", "\r").replace("\\n", "\n").encode()
    return re.sub(rb"\\x([0-9a-f]{2})", lambda m: bytes([int(m.group(1), 16)]), data)


def read_all(s):
    chunks = []
    while True:
        b = s.recv(65536)
        if not b:
            return b"".join(chunks)
        chunks.append(b)


def request_many(port, n, method, path):
    """Spec 016: n requests at once; their bodies, sorted, and whether all took at most 1 s."""
    import threading
    results = []
    lock = threading.Lock()

    def one():
        start = time.time()
        r = http_exchange(port, f"{method} {path} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode())
        with lock:
            results.append((r.split(b"\r\n\r\n", 1)[-1], time.time() - start))
    threads = [threading.Thread(target=one) for _ in range(n)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    bodies = sorted((b for b, _ in results), key=lambda b: (0, int(b)) if b.strip().isdigit() else (1, b))
    fast = all(dt <= 1.0 for _, dt in results)
    return b"\n".join(bodies) + b"\nwithin 1 s: " + (b"yes" if fast else b"no") + b"\n"


def http_exchange(port, data):
    """Sends data, reads until the server closes; b"(refused)" when nothing listens."""
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=30) as s:
            s.sendall(data)
            s.shutdown(socket.SHUT_WR)
            chunks = []
            while True:
                b = s.recv(65536)
                if not b:
                    break
                chunks.append(b)
            return b"".join(chunks)
    except ConnectionRefusedError:
        return b"(refused)"
    except OSError as e:
        return b"(" + type(e).__name__.encode() + b")"


def cases(paths):
    found = []
    for p in paths:
        for d, dirs, files in os.walk(os.path.abspath(p)):
            if "steps" in files:
                found.append(d)
                dirs[:] = []
            dirs[:] = [x for x in dirs if x != "expected"]
    return sorted(found)


def run_case(case):
    """Runs one case; returns {relative path: bytes} of the expected files it produced, plus 'out'."""
    work = tempfile.mkdtemp()
    servers = []
    opened = {}  # spec 016: request-open connections by name (closed at the end of the case)
    try:
        for n in os.listdir(case):
            if n in ("expected", "steps"):
                continue
            src = os.path.join(case, n)
            (shutil.copytree if os.path.isdir(src) else shutil.copy)(src, os.path.join(work, n))
        vport = free_port()  # spec 011: the port a started vo server may use; {vport} in case files too
        for d, _, fs in os.walk(work):
            for f in fs:
                if f.endswith((".vo", ".jsonl", ".effects")):
                    fp = os.path.join(d, f)
                    data = open(fp, "rb").read()
                    if b"{vport}" in data:
                        open(fp, "wb").write(data.replace(b"{vport}", str(vport).encode()))
        out = b""
        keep = []
        hidden = []
        port = None  # the HTTP test server's port, once started
        started = None  # (process, stdout file, stderr file) of `start vo`
        for line in open(os.path.join(case, "steps"), encoding="utf-8").read().splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            line_sub = line.replace("{vport}", str(vport))
            words = shlex.split(line_sub.replace("{port}", str(port)) if port else line_sub)
            if words[0] == "start":
                o, e = tempfile.TemporaryFile(), tempfile.TemporaryFile()
                proc = subprocess.Popen([VO] + words[2:], cwd=work, stdin=subprocess.DEVNULL, stdout=o, stderr=e)
                started = (proc, o, e)
                servers.append(proc)
                wait_listening(vport, proc)
                continue
            if words[0] in ("request", "request-raw"):
                if words[0] == "request":
                    body = words[3].encode() if len(words) > 3 else b""
                    data = (f"{words[1]} {words[2]} HTTP/1.1\r\nHost: localhost\r\n"
                            + (f"Content-Length: {len(body)}\r\n" if body else "") + "\r\n").encode() + body
                else:
                    text = line_sub.split(None, 1)[1]
                    text = re.sub(r"\{bytes:(\d+)\}", lambda m: "a" * int(m.group(1)), text)
                    data = text.replace("\\r", "\r").replace("\\n", "\n").encode()
                    data = re.sub(rb"\\x([0-9a-f]{2})", lambda m: bytes([int(m.group(1), 16)]), data)
                out += b"$ " + line.encode() + b"\n" + mask(http_exchange(vport, data), port, vport) + b"\n--- end\n"
                continue
            if words[0] == "request-open":  # spec 016: a connection left open (a slow or stalled client)
                s = socket.create_connection(("127.0.0.1", vport), timeout=60)
                s.sendall(raw_bytes(line_sub.split(None, 2)[2]))
                opened[words[1]] = s
                continue
            if words[0] == "read-open":
                s = opened.pop(words[1])
                try:
                    got = read_all(s)
                except OSError as e:
                    got = b"(" + type(e).__name__.encode() + b")"
                s.close()
                out += b"$ " + line.encode() + b"\n" + mask(got, port, vport) + b"\n--- end\n"
                continue
            if words[0] == "request-many":
                out += b"$ " + line.encode() + b"\n" + mask(request_many(vport, int(words[1]), words[2], words[3]), port, vport) \
                    + b"--- end\n"
                continue
            if words[0] == "no-children":  # spec 016: no task outlives its program
                def left():
                    n = 0
                    for pid in os.listdir("/proc"):
                        if pid.isdigit():
                            try:
                                n += os.path.realpath(os.readlink(f"/proc/{pid}/cwd")) == os.path.realpath(work)
                            except OSError:
                                pass
                    return n
                end = time.time() + 2
                while left() and time.time() < end:
                    time.sleep(0.05)
                out += b"$ " + line.encode() + b"\nleft-over processes: " + str(left()).encode() + b"\n"
                continue
            if words[0] == "wait":  # spec 011: let time pass (a server idle past --timeout)
                time.sleep(int(words[1]) / 1000)
                continue
            if words[0] == "signal":
                proc, o, e = started
                proc.send_signal(signal.SIGTERM if words[1] == "TERM" else signal.SIGINT)
                code = proc.wait(timeout=60)
                o.seek(0)
                e.seek(0)
                out += b"$ " + line.encode() + b"\n" + mask(o.read(), port, vport) + b"--- stderr\n" \
                    + mask(e.read(), port, vport) + b"--- exit " + str(code).encode() + b"\n"
                continue
            if words[0] == "server":
                cmd = [sys.executable, os.path.join(ROOT, "tests", "http_server.py"), "0"]
                if words[1:2] == ["https"]:  # spec 019: certificate NAME and options
                    cmd = [sys.executable, os.path.join(ROOT, "tests", "https_server.py"), "--cert", words[2]]
                    cmd += [os.path.join(work, w) if prev == "--log" else w
                            for prev, w in zip(words[2:], words[3:])]
                server = subprocess.Popen(cmd, stdout=subprocess.PIPE)
                servers.append(server)
                port = int(server.stdout.readline().split()[1])
                continue
            if words[0] == "stop-server":
                for sv in servers:
                    sv.kill()
                    sv.wait()
                continue
            if words[0] == "keep":
                keep += words[1:]
                continue
            if words[0] == "same":
                ok = len(hidden) >= 2 and hidden[-1] == hidden[-2]
                out += b"same stdout: " + (b"yes" if ok else b"no") + b"\n"
                continue
            if words[0] == "rm":
                p = os.path.join(work, words[1])
                shutil.rmtree(p) if os.path.isdir(p) and not os.path.islink(p) else os.remove(p)
                continue
            if words[0] == "cp":
                shutil.copy(os.path.join(work, words[1]), os.path.join(work, words[2]))
                continue
            if words[0] == "symlink":  # spec 013: symlink TARGET LINK (TARGET as written, LINK in the work dir)
                os.symlink(words[1], os.path.join(work, words[2]))
                continue
            if words[0] == "mkdir":
                os.makedirs(os.path.join(work, words[1]), exist_ok=True)
                continue
            if words[0] == "helpers":  # spec 017: the helper programs of tests/process_helpers/ in ./bin/
                os.makedirs(os.path.join(work, "bin"), exist_ok=True)
                hdir = os.path.join(ROOT, "tests", "process_helpers")
                for h in os.listdir(hdir):
                    shutil.copy(os.path.join(hdir, h), os.path.join(work, "bin", h))
                continue
            if words[0] == "touch":  # spec 017: an empty file whose name may hold any byte (\\xNN)
                name = re.sub(rb"\\x([0-9a-f]{2})", lambda m: bytes([int(m.group(1), 16)]), line.split()[1].encode())
                open(os.path.join(os.fsencode(work), name), "wb").close()
                continue
            if words[0] == "mkfifo":
                os.mkfifo(os.path.join(work, words[1]))
                continue
            if words[0] == "chmod":
                os.chmod(os.path.join(work, words[2]), int(words[1], 8))
                continue
            if words[0] == "write":
                text = line.split(None, 2)[2] if len(line.split(None, 2)) > 2 else ""
                text = re.sub(r"\{bytes:(\d+)\}", lambda m: "a" * int(m.group(1)), text)
                text = re.sub(r"\\x([0-9a-f]{2})", lambda m: chr(int(m.group(1), 16)), text)
                path = os.path.join(work, words[1])
                os.makedirs(os.path.dirname(path), exist_ok=True)
                open(path, "w", encoding="utf-8").write(text.replace("\\n", "\n"))
                continue
            env = dict(os.environ)
            while words[0] in ("env", "unset"):
                kind = words.pop(0)
                while words and words[0] not in ("vo", "vo~", "app", "app~", "env", "unset"):
                    w = words.pop(0)
                    if kind == "env":
                        k, v = w.split("=", 1)
                        # spec 021: \xNN is that byte (values that are not UTF-8)
                        env[k] = os.fsdecode(re.sub(rb"\\x([0-9a-f]{2})", lambda m: bytes([int(m.group(1), 16)]), v.encode()))
                    else:
                        env.pop(w, None)
            jail = words[0] == "no-trust-store"  # spec 019
            if jail:
                words.pop(0)
            hide = words[0] in ("vo~", "app~")  # app~ (spec 035): app with its stdout left out
            if hide:
                words[0] = words[0][:-1]
            if words[0] == "app":  # spec 034: a built executable, run the way bin/vo runs $VO_BIN
                env["VO_BIN"] = os.path.join(work, words[1])
                words[0:2] = ["vo"]
            assert words[0] == "vo", f"{case}: unknown step {line!r}"
            if words[1:2] == ["replay"] and os.environ.get("VO_REPLAY_BIN"):
                env["VO_BIN"] = os.environ["VO_REPLAY_BIN"]  # tests/cross_replay.py: another build replays
            stdin = b""
            if len(words) >= 2 and words[-2] == "<":
                stdin = open(os.path.join(work, words[-1]), "rb").read()
                words = words[:-2]
            argv = [VO] + words[1:]
            if jail:
                ok = subprocess.run(["unshare", "-rm", "true"], capture_output=True).returncode == 0
                if not ok:
                    out += b"$ " + line.encode() + b"\nskipped: no user namespaces\n"
                    continue
                argv = ["unshare", "-rm", "sh", "-c", "mount -t tmpfs none /etc/ssl; [ -d /etc/pki ] && "
                        "mount -t tmpfs none /etc/pki; exec \"$@\"", "sh"] + argv
            r = subprocess.run(argv, cwd=work, input=stdin, capture_output=True, env=env, timeout=120)
            if hide:
                hidden.append(r.stdout)
            out += b"$ " + line.encode() + b"\n" + (b"(stdout not shown)\n" if hide else mask(r.stdout, port, vport)) \
                + b"--- stderr\n" + mask(r.stderr, port, vport) + b"--- exit " + str(r.returncode).encode() + b"\n"
        got = {"out": out.replace(work.encode() + b"/", b"")}
        for rel in keep:
            p = os.path.join(work, rel)
            got[rel] = mask(open(p, "rb").read(), port, vport) if os.path.isfile(p) else None
        return got
    finally:
        for s in opened.values():
            s.close()
        for sv in servers:
            sv.kill()
            sv.wait()
        shutil.rmtree(work, ignore_errors=True)


def expected(case):
    exp = os.path.join(case, "expected")
    want = {}
    if os.path.isdir(exp):
        for d, _, files in os.walk(exp):
            for n in files:
                p = os.path.join(d, n)
                want[os.path.relpath(p, exp)] = mask(open(p, "rb").read())
    return want


def diff(a, b):
    import difflib
    a = (a or b"").decode(errors="replace").splitlines()
    b = (b or b"").decode(errors="replace").splitlines()
    return "\n".join(list(difflib.unified_diff(a, b, "expected", "actual", lineterm=""))[:40])


def main():
    args = sys.argv[1:]
    accept = "--accept" in args
    repeat = 1
    if "--repeat" in args:
        i = args.index("--repeat")
        repeat = int(args[i + 1])
        del args[i:i + 2]
    args = [a for a in args if a != "--accept"]
    passed = failed = 0
    for case in cases(args or [os.path.join(ROOT, "tests", "cases")]):
        name = os.path.relpath(case, ROOT)
        got = run_case(case)
        bad = [k for k in sorted(set(expected(case)) | set(got)) if expected(case).get(k) != got.get(k)]
        for _ in range(repeat - 1):
            if bad:
                break
            if run_case(case) != got:
                bad = ["(repeat run differs)"]
        if not bad:
            print("PASS", name)
            passed += 1
        elif accept and bad != ["(repeat run differs)"]:
            exp = os.path.join(case, "expected")
            os.makedirs(exp, exist_ok=True)
            for k, v in got.items():
                p = os.path.join(exp, k)
                if v is None:
                    if os.path.exists(p):
                        os.remove(p)
                    continue
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, "wb").write(v)
            print("ACCEPT", name)
            passed += 1
        else:
            print("FAIL", name)
            for k in bad:
                if k.startswith("("):
                    print("   ", k)
                else:
                    print(f"    --- {k}:")
                    print("    " + diff(expected(case).get(k), got.get(k)).replace("\n", "\n    "))
            failed += 1
    print(f"passed {passed}, failed {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
