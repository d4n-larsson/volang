#!/usr/bin/env python3
"""Spec 021 SC-004, SC-008: binary data is as fast as text.

- 100 MB read, sliced in half and written as bytes takes at most twice the time of the same as text
  (an ASCII file, so both paths work), within the default memory limit;
- to_hex and to_base64 of 100 MB take under 1 s each (the time of a run minus that of the read alone);
- a 10 MB download with get_bytes takes at most the time of get on the same text body, plus 10%.

Each figure is the median of 3 whole `vo run` processes.
"""
import os, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))
MB = 1 << 20

FILE_TEXT = """fn main() -> result[void] uses console, file.read("./"), file.write("./") {
    let s = try read_file("big.txt");
    try write_file("out.txt", slice(s, 0, len(s) / 2));
    print(str(len(s)));
    return ok();
}
"""
FILE_BYTES = """import std.fs;

fn main() -> result[void] uses console, file.read("./"), file.write("./") {
    let b = try fs.read_bytes("big.txt");
    try fs.write_bytes("out.bin", slice(b, 0, len(b) / 2));
    print(str(len(b)));
    return ok();
}
"""
CODEC = """import std.fs;
import std.bytes;

fn main() -> result[void] uses console, file.read("./") {{
    let b = try fs.read_bytes("big.txt");
    print(str(len({expr})));
    return ok();
}}
"""
GET = """import std.http;

fn main() -> result[void] uses console, net("127.0.0.1") {{
    let r = try http.{fn}(args()[0]);
    print(str(len(r.body)));
    return ok();
}}
"""


def median_s(d, name, src, args=()):
    open(os.path.join(d, name), "w").write(src)
    times = []
    for _ in range(3):
        t = time.monotonic()
        p = subprocess.run([VO, "run", "--timeout", "120", name, "--", *args], cwd=d, capture_output=True, text=True)
        times.append(time.monotonic() - t)
        if p.returncode:
            print(f"FAIL {name}: exit {p.returncode}\n{p.stderr[:600]}")
            sys.exit(1)
    return statistics.median(times)


def main():
    fails = []
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "big.txt"), "wb") as f:
            f.write((b"0123456789abcdef" * (MB // 16)) * 100)
        text = median_s(d, "ft.vo", FILE_TEXT)
        raw = median_s(d, "fb.vo", FILE_BYTES)
        print(f"bytes_scale: 100 MB read, slice, write: text {text:.2f} s, bytes {raw:.2f} s (limit 2x)")
        if raw > 2 * text:
            fails.append("bytes file path over 2x text")
        read = median_s(d, "r.vo", CODEC.format(expr="b"))
        for fn in ("to_hex", "to_base64"):
            t = median_s(d, fn + ".vo", CODEC.format(expr=f"bytes.{fn}(b)")) - read
            print(f"bytes_scale: {fn} of 100 MB: {t:.2f} s (limit 1 s)")
            if t >= 1.0:
                fails.append(f"{fn} took {t:.2f} s")
        srv = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "http_server.py")], stdout=subprocess.PIPE)
        try:
            port = int(srv.stdout.readline().split()[1])
            url = f"http://127.0.0.1:{port}/big"
            g = median_s(d, "gt.vo", GET.format(rec="Response", fn="get"), [url])
            gb = median_s(d, "gb.vo", GET.format(rec="BytesResponse", fn="get_bytes"), [url])
        finally:
            srv.kill()
        print(f"bytes_scale: 10 MB download: get {g:.2f} s, get_bytes {gb:.2f} s (limit get + 10%)")
        if gb > g * 1.1 + 0.02:
            fails.append("get_bytes slower than get")
    for f in fails:
        print("FAIL", f)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
