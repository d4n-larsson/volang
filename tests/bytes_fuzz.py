#!/usr/bin/env python3
"""Spec 021 SC-007: no text operation can see invalid UTF-8.

Seeded random byte strings (half valid UTF-8 with corruptions, half random bytes) go through every
path where outside data becomes a str: read_file, read_line, read_all, args, env, a program's text
output (std.process.run) and an HTTP text body (std.http.get). Each time vo must either return the text
Python's strict decoder returns, or report the offset where that decoder's error starts (the process
error names no offset: it must only be an error exactly when the bytes are invalid).

  bytes_fuzz.py [--count N] [--runs M] [--seed S]
N inputs for the paths that read many inputs in one run (read_file, read_line, process, HTTP; default
10,000); M inputs for those that need one run each (read_all, args, env; default 1,000).
"""
import os, random, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import steps  # noqa: E402  (free_port, wait_listening)

PIECES = ["a", "é", "€", "😀", "߿", "￿", "\U0010ffff", "Z", " "]


def sample(rng, no=b""):
    if rng.random() < 0.5:  # valid text, then a corruption or two
        b = bytearray("".join(rng.choice(PIECES) for _ in range(rng.randint(0, 12))).encode())
        for _ in range(rng.randint(0, 2)):
            if not b:
                break
            i = rng.randrange(len(b))
            k = rng.random()
            if k < 0.4:
                b[i] = rng.randrange(256)
            elif k < 0.7:
                del b[i]
            else:
                b.insert(i, rng.choice([0x80, 0xc0, 0xe0, 0xed, 0xf0, 0xf4, 0xf5, 0xff]))
    else:
        b = bytearray(rng.randrange(256) for _ in range(rng.randint(0, 24)))
    return bytes(c for c in b if c not in no)


def expect(b):
    try:
        return "ok", b.decode("utf-8")
    except UnicodeDecodeError as e:
        return "err", e.start


OFFSET = re.compile(r"at byte (\d+)")
SHOW = """import std.bytes.from_text;
import std.bytes.to_hex;

fn show(r: result[str]) uses console {
    match r {
        case ok(s) {
            print("ok " + to_hex(from_text(s)));
        }
        case err(e) {
            print("err " + e);
        }
    }
}
"""


def got(line):
    if line.startswith("ok "):
        return "ok", bytes.fromhex(line[3:]).decode("utf-8")
    m = OFFSET.search(line)
    return "err", int(m.group(1)) if m else None


def run(args, **kw):
    return subprocess.run([VO, "run", "--timeout", "600"] + args, capture_output=True, **kw)


class Fuzz:
    def __init__(self, d):
        self.d, self.failures, self.checked = d, [], 0

    def compare(self, path, b, have, offsets=True):
        want = expect(b)
        self.checked += 1
        ok = have == want if offsets else have[0] == want[0] and (want[0] == "err" or have == want)
        if not ok and len(self.failures) < 10:
            self.failures.append(f"{path}: {b.hex() or '(empty)'}: want {want}, got {have}")
        return ok

    def program(self, name, text):
        p = os.path.join(self.d, name)
        open(p, "w").write(text)
        return p

    def files(self, inputs):
        names = []
        for i, b in enumerate(inputs):
            names.append(os.path.join(self.d, f"in{i}.bin"))
            open(names[-1], "wb").write(b)
        open(os.path.join(self.d, "names.txt"), "w").write("\n".join(names) + "\n")
        return names

    def read_file(self, inputs):
        self.files(inputs)
        p = self.program("rf.vo", SHOW + """
fn main() -> result[void] uses console, file.read {
    let names = try read_file("names.txt");
    for name in split(names, "\\n") {
        if name != "" {
            show(read_file(name));
        }
    }
    return ok();
}
""")
        out = run([p], cwd=self.d, text=True).stdout.splitlines()
        for b, line in zip(inputs, out):
            self.compare("read_file", b, got(line))
        if len(out) != len(inputs):
            self.failures.append(f"read_file: {len(out)} results for {len(inputs)} inputs")

    def read_line(self, inputs):
        p = self.program("rl.vo", SHOW + """
fn main() uses console, input {
    while true {
        let r = read_line();
        if r == err("eof") {
            return;
        }
        show(r);
    }
}
""")
        out = run([p], cwd=self.d, input=b"".join(b + b"\n" for b in inputs)).stdout.decode().splitlines()
        for b, line in zip(inputs, out):
            self.compare("read_line", b, got(line))
        if len(out) != len(inputs):
            self.failures.append(f"read_line: {len(out)} results for {len(inputs)} inputs")

    def one_run(self, path, inputs, src, how):
        p = self.program(path + ".vo", SHOW + src)
        for b in inputs:
            r = how(p, b)
            line = r.stdout.decode(errors="replace").rstrip("\n") or r.stderr.decode(errors="replace")
            self.compare(path, b, got(line))

    def process(self, inputs):
        names = self.files(inputs)
        os.makedirs(os.path.join(self.d, "bin"), exist_ok=True)
        shutil.copy(os.path.join(ROOT, "tests", "process_helpers", "cat_file"), os.path.join(self.d, "bin"))
        p = self.program("pr.vo", SHOW + """
import std.process.Output;
import std.process.run;

fn main() -> result[void] uses console, file.read, process("bin/cat_file") {
    let names = try read_file("names.txt");
    for name in split(names, "\\n") {
        if name != "" {
            match run("bin/cat_file", [name]) {
                case ok(o) {
                    show(ok(o.stdout));
                }
                case err(e) {
                    show(err(e));
                }
            }
        }
    }
    return ok();
}
""")
        out = run([p], cwd=self.d, text=True).stdout.splitlines()
        for b, line in zip(inputs, out):
            self.compare("process", b, got(line), offsets=False)
        if len(out) != len(inputs):
            self.failures.append(f"process: {len(out)} results for {len(inputs)} inputs")

    def http(self, inputs):
        srv = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "http_server.py")], stdout=subprocess.PIPE)
        try:
            port = int(srv.stdout.readline().split()[1])
            open(os.path.join(self.d, "urls.txt"), "w").write(
                "".join(f"http://127.0.0.1:{port}/hex/{b.hex()}\n" for b in inputs))
            p = self.program("ht.vo", SHOW + """
import std.http.Response;
import std.http.get;

fn main() -> result[void] uses console, file.read, net("127.0.0.1") {
    let urls = try read_file("urls.txt");
    for url in split(urls, "\\n") {
        if url != "" {
            match get(url) {
                case ok(r) {
                    show(ok(r.body));
                }
                case err(e) {
                    show(err(e));
                }
            }
        }
    }
    return ok();
}
""")
            out = run([p], cwd=self.d, text=True).stdout.splitlines()
        finally:
            srv.kill()
        for b, line in zip(inputs, out):
            self.compare("http", b, got(line))
        if len(out) != len(inputs):
            self.failures.append(f"http: {len(out)} results for {len(inputs)} inputs")


def main():
    a = sys.argv[1:]
    count = int(a[a.index("--count") + 1]) if "--count" in a else 10000
    runs = int(a[a.index("--runs") + 1]) if "--runs" in a else 1000
    seed = int(a[a.index("--seed") + 1]) if "--seed" in a else 21
    rng = random.Random(seed)
    with tempfile.TemporaryDirectory() as d:
        f = Fuzz(d)
        f.read_file([sample(rng) for _ in range(count)])
        f.read_line([sample(rng, no=b"\r\n") for _ in range(count)])
        f.process([sample(rng) for _ in range(count)])
        f.http([sample(rng) for _ in range(count)])
        f.one_run("read_all", [sample(rng) for _ in range(runs)], """
fn main() uses console, input {
    show(ok(read_all()));
}
""", lambda p, b: run([p], cwd=d, input=b))
        f.one_run("args", [sample(rng, no=b"\0") for _ in range(runs)], """
fn main() uses console {
    show(ok(args()[0]));
}
""", lambda p, b: subprocess.run([VO, "run", p, "--", os.fsdecode(b)], cwd=d, capture_output=True))
        f.one_run("env", [sample(rng, no=b"\0") for _ in range(runs)], """
fn main() uses console, env("FUZZ") {
    show(env("FUZZ"));
}
""", lambda p, b: subprocess.run([VO, "run", p], cwd=d, capture_output=True,
                                 env=dict(os.environ, FUZZ=os.fsdecode(b))))
    for x in f.failures:
        print("FAIL", x)
    print(f"bytes_fuzz: {f.checked} inputs through 7 text paths, {len(f.failures)} failures")
    return 1 if f.failures else 0


if __name__ == "__main__":
    sys.exit(main())
