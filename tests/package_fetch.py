#!/usr/bin/env python3
"""Spec 041: `vo package ENTRY NAME URL` and `--restore` against a local HTTPS server.

  package_fetch.py                 every group (part of `make test`)
  package_fetch.py --group NAME    one group: corpus, fetch, hostile, quiet, restore, effects, fuzz

Archives are written to a temporary directory that tests/https_server.py serves (VO_ARCHIVES) with the
test authority's certificate. Each case runs bin/vo ($VO_BIN) in a fresh project.
"""
import gzip
import io
import json
import os
import random
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
CA = os.path.join(ROOT, "tests", "tls", "certs", "test-ca.pem")
SERVER = os.path.join(ROOT, "tests", "https_server.py")
MAIN = "fn main() {\n}\n"
failures = []
cases = 0


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


class Server:
    def __init__(self, archives, cert="a.test"):
        env = dict(os.environ, VO_ARCHIVES=archives)
        self.log = os.path.join(archives, f"log-{cert}.txt")
        self.p = subprocess.Popen([sys.executable, SERVER, "--cert", cert, "--log", self.log], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
        self.port = int(self.p.stdout.readline().split()[1])
        self.base = f"https://127.0.0.1:{self.port}"

    def stop(self):
        self.p.kill()
        self.p.wait()
        self.p.stdout.close()


def vo(d, words, timeout=120):
    r = subprocess.run([VO] + words, cwd=d, capture_output=True, text=True, timeout=timeout * SLOW)
    return r.returncode, r.stdout, r.stdout + r.stderr  # (errors go to stdout or stderr by command: both)


def project(base, name="proj"):
    d = os.path.join(base, name)
    os.makedirs(d)
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(MAIN)
    return d


def add(d, name, url, *more):
    return vo(d, ["package", "--ca-file", CA] + list(more) + ["main.vo", name, url])


def snapshot(d):
    out = {}
    for root, dirs, files in os.walk(d):
        for n in dirs:
            out[os.path.relpath(os.path.join(root, n), d) + "/"] = b""
        for n in files:
            p = os.path.join(root, n)
            out[os.path.relpath(p, d)] = os.readlink(p).encode() if os.path.islink(p) else open(p, "rb").read()
    return out


def tar_bytes(files, fmt=tarfile.PAX_FORMAT, level=6, top=None):
    """An archive of {path: bytes}; level None: not compressed."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=fmt) as t:
        if top:
            info = tarfile.TarInfo(top)
            info.type = tarfile.DIRTYPE
            info.mode = 0o755
            t.addfile(info)
        for path, data in files.items():
            info = tarfile.TarInfo((top + "/" if top else "") + path)
            info.size = len(data)
            t.addfile(info, io.BytesIO(data))
    if level is None:
        return raw.getvalue()
    out = io.BytesIO()
    with gzip.GzipFile(fileobj=out, mode="wb", compresslevel=level, mtime=0) as g:
        g.write(raw.getvalue())
    return out.getvalue()


def header(name, size=0, kind=b"0", link="", prefix="", checksum=None, size_field=None):
    """One 512-byte ustar header, written by hand so that it can be wrong."""
    h = bytearray(512)
    h[0:len(name)] = name.encode("latin-1")[:100]
    h[100:108] = b"0000644\0"
    h[108:116] = h[116:124] = b"0000000\0"
    h[124:136] = size_field if size_field else b"%011o\0" % size
    h[136:148] = b"00000000000\0"
    h[148:156] = b"        "
    h[156:157] = kind
    h[157:157 + len(link)] = link.encode()
    h[257:265] = b"ustar\x0000"
    h[345:345 + len(prefix)] = prefix.encode()
    total = sum(h) if checksum is None else checksum
    h[148:156] = b"%06o\0 " % total
    return bytes(h)


def entry(name, data=b"", **kw):
    pad = (512 - len(data) % 512) % 512
    return header(name, len(data), **kw) + data + b"\0" * pad


END = b"\0" * 1024
GOOD = entry("pkg/a.vo", b"export fn a() -> int {\n    return 1;\n}\n")


def gz(data, level=6):
    return gzip.compress(data, compresslevel=level, mtime=0)


def local_pin(base, files, n):
    """The hash `vo package` gives the same .vo files unpacked by hand."""
    d = os.path.join(base, f"local{n}")
    for path, data in files.items():
        if path.endswith(".vo"):
            p = os.path.join(d, "tree", path)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, "wb").write(data)
    open(os.path.join(d, "main.vo"), "w").write(MAIN)
    code, out, err = vo(d, ["package", "main.vo", "p", "tree"])
    m = re.search(r"sha256:[0-9a-f]{64}", out)
    return m.group(0) if m else out + err


def vo_file(n):
    return f"export fn f{n}() -> int {{\n    return {n};\n}}\n".encode()


def group_corpus(base, arch, srv):
    rnd = random.Random(41)
    formats = [tarfile.USTAR_FORMAT, tarfile.GNU_FORMAT, tarfile.PAX_FORMAT]
    bad = 0
    count = 200 if SLOW == 1 else 30
    for i in range(count):
        files = {}
        deep = "lib/pkg/" if i % 7 == 0 else ""
        for k in range(rnd.randint(1, 6)):
            folder = "/".join(rnd.choice(["a", "bb", "core", "x" * (120 if i % 3 else 90) if i % 5 == 0 else "util"])
                              for _ in range(rnd.randint(0, 3)))
            files[deep + (folder + "/" if folder else "") + f"m{k}.vo"] = vo_file(k)
        if i % 4 == 0:
            files[deep + "empty.vo"] = b""
        if i == 3:
            files[deep + "bytes.vo"] = bytes(range(256)) * 4
        if i == 11:
            files[deep + "big.vo"] = b"# filler line of a large module\n" * (5 * 1024 * 1024 // 32)
        files[deep + "README.md"] = b"not a module\n"
        files["LICENSE"] = b"text\n"
        if deep:
            files["other/skip.vo"] = vo_file(99)  # outside #PATH: not part of the package
        fmt = formats[i % 3]
        level = [None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9][i % 11]
        top = f"repo-{i}.0" if i % 2 else None
        name = f"c{i}.tar" + ("" if level is None else ".gz")
        if i % 10 == 9:  # the system's tar
            src = os.path.join(base, f"src{i}", top or "flat")
            for path, data in files.items():
                p = os.path.join(src, path)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, "wb").write(data)
            what = [top] if top else sorted(os.listdir(src))
            subprocess.run(["tar", "-C", os.path.dirname(src) if top else src, "-c" + ("" if level is None else "z") + "f",
                            os.path.join(arch, name)] + what, check=True)
        else:
            open(os.path.join(arch, name), "wb").write(tar_bytes(files, fmt, level, top))
        want = {p[len(deep):]: d for p, d in files.items() if p.startswith(deep) and p.endswith(".vo")}
        d = project(base, f"corpus{i}")
        code, out, err = add(d, "p", f"{srv.base}/archive/{name}" + ("#lib/pkg" if deep else ""))
        got = {k: v for k, v in snapshot(os.path.join(d, "packages", "p")).items() if not k.endswith("/")} \
            if os.path.isdir(os.path.join(d, "packages", "p")) else {}
        m = re.search(r"sha256:[0-9a-f]{64}", out)
        if code != 0 or got != want or not m or m.group(0) != local_pin(base, want, i):
            bad += 1
            if bad <= 3:
                print(f"  corpus {i} ({name}): exit {code} {out[:200]} {err[:300]} files {sorted(got)} want {sorted(want)}")
        shutil.rmtree(d)
    case(f"corpus/{count} archives unpack to the same files and hash as a local pin (SC-001, SC-005)", bad == 0, f"{bad} differ")


def group_fetch(base, arch, srv):
    files = {"a.vo": vo_file(1), "sub/b.vo": vo_file(2)}
    open(os.path.join(arch, "one.tar.gz"), "wb").write(tar_bytes(files, top="one-1.0"))
    open(os.path.join(arch, "two.tar.gz"), "wb").write(tar_bytes({"a.vo": vo_file(7)}, top="one-2.0"))
    d = project(base, "fetch")
    code, out, err = add(d, "one", f"{srv.base}/moved/one.tar.gz")
    case("fetch/a redirect is followed", code == 0 and out.startswith("added one packages/one sha256:"), out + err)
    line = open(os.path.join(d, "vo.packages")).read().splitlines()[-1]
    case("fetch/the manifest line has the URL", line.endswith(f" {srv.base}/moved/one.tar.gz") and line.startswith("one packages/one sha256:"), line)
    code, out, err = add(d, "one", f"{srv.base}/moved/one.tar.gz")
    case("fetch/the same archive again is unchanged", (code, out) == (0, "unchanged one\n"), out + err)
    code, out, err = add(d, "one", f"{srv.base}/hops/4/one.tar.gz")
    case("fetch/five redirects are followed", code == 0, out + err)
    for what, url, code_want in (("six redirects", f"{srv.base}/hops/5/one.tar.gz", "E0706"),
                                 ("a redirect to http", f"{srv.base}/insecure/one.tar.gz", "E0706"),
                                 ("a 404", f"{srv.base}/archive/none.tar.gz", "E0706"),
                                 ("a refused connection", "https://127.0.0.1:1/x.tar.gz", "E0706")):
        before = snapshot(d)
        code, out, err = add(d, "one", url)
        case(f"fetch/{what} fails and changes nothing", code == 65 and code_want in err and snapshot(d) == before, f"{code} {err[:200]}")
    for what, url in (("http://", "http://127.0.0.1/x.tar.gz"), ("credentials", "https://u:p@127.0.0.1/x.tar.gz"),
                      ("#../x", f"{srv.base}/archive/one.tar.gz#../x"), ("#/x", f"{srv.base}/archive/one.tar.gz#/x"),
                      ("#a//b", f"{srv.base}/archive/one.tar.gz#a//b"), ("an empty #", f"{srv.base}/archive/one.tar.gz#")):
        code, out, err = add(d, "one", url)
        case(f"fetch/{what} is a usage error", code == 64 and "T0001" in err, f"{code} {err[:200]}")
    bad = Server(arch, "expired")
    code, out, err = add(d, "one", f"{bad.base}/archive/one.tar.gz")
    bad.stop()
    case("fetch/a bad certificate is refused with the reason", code == 65 and "E0706" in err and "has expired" in err, err[:300])
    open(srv.log, "w").close()
    code, out, err = add(d, "one", f"{srv.base}/archive/one.tar.gz?ref=v1#sub")
    log = open(srv.log).read()
    case("fetch/the query is sent, the #PATH is not", code == 0 and "/archive/one.tar.gz?ref=v1" in log and "#" not in log, f"{code} {out}{err} {log}")
    case("fetch/#PATH takes that folder", sorted(os.listdir(os.path.join(d, "packages", "one"))) == ["b.vo"], str(os.listdir(os.path.join(d, "packages", "one"))))
    code, out, err = add(d, "one", f"{srv.base}/archive/two.tar.gz")
    case("fetch/an update shows both hashes and replaces the directory", code == 0 and re.match(r"updated one: sha256:\w+ -> sha256:\w+\n", out)
         and sorted(os.listdir(os.path.join(d, "packages", "one"))) == ["a.vo"], out + err)
    open(os.path.join(d, "packages", "one", "a.vo"), "a").write("# my change\n")
    before = snapshot(d)
    code, out, err = add(d, "one", f"{srv.base}/archive/one.tar.gz")
    case("fetch/an edited package directory is not replaced (E0701)", code == 65 and "E0701" in err and snapshot(d) == before, f"{code} {err[:300]}")
    d2 = project(base, "fetchdir")
    code, out, err = add(d2, "one", f"{srv.base}/archive/one.tar.gz", "--dir", "vendor/m")
    case("fetch/--dir writes there", code == 0 and os.path.isfile(os.path.join(d2, "vendor", "m", "a.vo"))
         and "one vendor/m sha256:" in open(os.path.join(d2, "vo.packages")).read(), out + err)
    d3 = project(base, "fetchtaken")
    os.makedirs(os.path.join(d3, "packages", "one"))
    open(os.path.join(d3, "packages", "one", "mine.txt"), "w").write("keep me\n")
    before = snapshot(d3)
    code, out, err = add(d3, "one", f"{srv.base}/archive/one.tar.gz")
    case("fetch/a directory that is not the package's is refused", code == 65 and "E0706" in err and snapshot(d3) == before, f"{code} {err[:200]}")
    code, out, err = vo(d2, ["package", "main.vo", "one", "vendor/m"])
    case("fetch/pinning by hand drops the URL", code == 0 and "https://" not in open(os.path.join(d2, "vo.packages")).read(), out + err)
    code, out, err = vo(d2, ["package", "--dir", "x", "main.vo", "one", "vendor/m"])
    case("fetch/--dir without a URL is a usage error", code == 64, f"{code} {err[:200]}")


def hostile_archives():
    big = b"# x\n" * (17 << 18)
    return {
        "a path with ../": entry("pkg/../../evil.vo", b"x"),
        "an absolute path": entry("/tmp/evil.vo", b"x"),
        "a symbolic link": GOOD + header("pkg/link.vo", kind=b"2", link="/etc/passwd"),
        "a hard link": GOOD + header("pkg/hard.vo", kind=b"1", link="pkg/a.vo"),
        "a character device": GOOD + header("pkg/dev.vo", kind=b"3"),
        "a FIFO": GOOD + header("pkg/fifo.vo", kind=b"6"),
        "a control byte in a path": entry("pkg/a\x01b.vo", b"x"),
        "a wrong header checksum": GOOD + header("pkg/b.vo", 0, checksum=1),
        "a base-256 size": header("pkg/b.vo", size_field=b"\x80" + b"\0" * 10 + b"\x05"),
        "a tar cut mid-file": header("pkg/b.vo", 4000) + b"x" * 700,
        "a file over 16 MiB": entry("pkg/big.vo", big),
        "10,001 entries": b"".join(header(f"pkg/e{i}.vo") for i in range(10001)),
        "not an archive": b"<html>not found</html>" * 40,
    }


def group_hostile(base, arch, srv):
    d = project(base, "hostile")
    open(os.path.join(arch, "good.tar.gz"), "wb").write(gz(GOOD + END))
    code, out, err = add(d, "pkg", f"{srv.base}/archive/good.tar.gz")
    case("hostile/a good archive first", code == 0, out + err)
    before = snapshot(d)
    made = {name: gz(data + END) for name, data in hostile_archives().items()}
    whole = gz(GOOD + END)
    made["a gzip cut mid-stream"] = whole[:len(whole) // 2]
    made["a wrong CRC-32"] = whole[:-8] + bytes([whole[-8] ^ 1]) + whole[-7:]
    made["a wrong length field"] = whole[:-4] + bytes([whole[-4] ^ 1]) + whole[-3:]
    made["a gzip that inflates past 64 MiB"] = gz(b"\0" * (65 << 20))
    made["a download over 16 MiB"] = random.Random(1).randbytes(17 << 20)
    e0702 = {"no .vo file": gz(entry("pkg/readme.txt", b"x") + END)}
    for n, (what, data) in enumerate(list(made.items()) + list(e0702.items())):
        open(os.path.join(arch, f"h{n}.tar.gz"), "wb").write(data)
        code, out, err = add(d, "pkg", f"{srv.base}/archive/h{n}.tar.gz")
        want = "E0702" if what in e0702 else "E0706"
        case(f"hostile/{what}: refused, nothing changed (SC-002)", code == 65 and want in err and snapshot(d) == before, f"{code} {err[:200]}")
    code, out, err = add(d, "pkg", f"{srv.base}/archive/good.tar.gz#nowhere")
    case("hostile/a #PATH that is not in the archive is E0702", code == 65 and "E0702" in err and "nowhere" in err and snapshot(d) == before, f"{code} {err[:200]}")
    evil = [p for p in ("/tmp/evil.vo", os.path.join(os.path.dirname(d), "evil.vo")) if os.path.exists(p)]
    case("hostile/nothing was written outside the project", not evil, str(evil))


def group_quiet(base, arch, srv):
    if not shutil.which("strace"):
        print("package_fetch: quiet skipped (no strace)")
        return
    open(os.path.join(arch, "q.tar.gz"), "wb").write(tar_bytes({"a.vo": b"export fn a() -> int {\n    return 1;\n}\n"}))
    d = project(base, "quiet")
    add(d, "q", f"{srv.base}/archive/q.tar.gz")
    open(os.path.join(d, "main.vo"), "w").write('import q.a;\n\nfn main() uses console {\n    print(str(a.a()));\n}\n\ntest "t" {\n    expect_eq(a.a(), 1);\n}\n')
    real = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))
    for present in (True, False):
        if not present:
            shutil.rmtree(os.path.join(d, "packages"))
        for words in (["check", "main.vo"], ["run", "main.vo"], ["test", "main.vo"], ["build", "main.vo", "-o", "app"], ["doc", "q.a"]):
            trace = os.path.join(d, "trace.txt")
            r = subprocess.run(["strace", "-f", "-e", "trace=socket,connect", "-o", trace, real] + words, cwd=d, capture_output=True, text=True)
            calls = [l for l in open(trace).read().splitlines() if "socket(" in l or "connect(" in l]
            os.remove(trace)
            ok = not calls and (r.returncode == 0 if present else "E0702" in r.stdout + r.stderr)
            case(f"quiet/vo {words[0]} opens no socket ({'package present' if present else 'package missing: E0702'}) (SC-006)", ok,
                 f"{r.returncode} {calls[:2]} {(r.stdout + r.stderr)[:200]}")


def group_restore(base, arch, srv):
    d = project(base, "restore")
    for n in ("r1", "r2", "r3"):
        open(os.path.join(arch, n + ".tar.gz"), "wb").write(tar_bytes({"m.vo": vo_file(len(n) + ord(n[1])), "s/t.vo": vo_file(2)}, top=n))
        add(d, n, f"{srv.base}/archive/{n}.tar.gz")
    os.makedirs(os.path.join(d, "local"))
    open(os.path.join(d, "local", "l.vo"), "wb").write(vo_file(5))
    vo(d, ["package", "main.vo", "loc", "local"])
    manifest = open(os.path.join(d, "vo.packages")).read()
    full = snapshot(d)
    shutil.rmtree(os.path.join(d, "packages"))
    code, out, err = vo(d, ["package", "--ca-file", CA, "--restore", "main.vo"])
    case("restore/three restored, one present (SC-003)", (code, out) == (0, "present loc\nrestored r1\nrestored r2\nrestored r3\n"), f"{code} {out}{err}")
    case("restore/the tree is back and the manifest untouched", snapshot(d) == full and open(os.path.join(d, "vo.packages")).read() == manifest)
    code, out, err = vo(d, ["check", "main.vo"])
    case("restore/vo check passes afterwards", code == 0, out + err)
    code, out, err = vo(d, ["package", "--ca-file", CA, "--restore", "main.vo"])
    case("restore/nothing to do", (code, out) == (0, "present loc\npresent r1\npresent r2\npresent r3\n"), f"{code} {out}{err}")
    shutil.rmtree(os.path.join(d, "packages"))
    open(os.path.join(arch, "r2.tar.gz"), "wb").write(tar_bytes({"m.vo": vo_file(1234)}, top="r2"))
    code, out, err = vo(d, ["package", "--ca-file", CA, "--restore", "main.vo"])
    case("restore/a changed upstream is E0701, the others are restored", code == 65 and "E0701" in err and "r2" in err
         and [l for l in out.splitlines() if not l.startswith("error")] == ["present loc", "restored r1", "restored r3"] and not os.path.exists(os.path.join(d, "packages", "r2"))
         and sorted(os.listdir(os.path.join(d, "packages"))) == ["r1", "r3"], f"{code} {out}{err[:300]}")
    case("restore/the manifest is still untouched", open(os.path.join(d, "vo.packages")).read() == manifest)
    shutil.rmtree(os.path.join(d, "local"))
    open(os.path.join(d, "packages", "r1", "m.vo"), "a").write("# edit\n")
    code, out, err = vo(d, ["package", "--json", "--ca-file", CA, "--restore", "main.vo"])
    recs = [json.loads(l) for l in out.splitlines() if l.startswith("{")]
    codes = sorted(r.get("code", r.get("change")) for r in recs)
    case("restore/missing local (E0702), edited (E0701), changed upstream (E0701), present; as JSON records", code == 65
         and codes == ["E0701", "E0701", "E0702", "present"] and "# edit" in open(os.path.join(d, "packages", "r1", "m.vo")).read(), f"{code} {codes} {out}{err[:300]}")


PURE = {"a.vo": b"export fn double(n: int) -> int {\n    return n * 2;\n}\n"}
SCOPED = {
    "io.vo": b'import std.http;\n\nexport effects Storage = file.write("out/");\n\n'
             b'export fn load(path: str) -> result[str] uses file.read("data/") {\n    return read_file("data/" + path);\n}\n\n'
             b'export fn save(text: str) -> result[void] uses Storage {\n    return write_file("out/x", text);\n}\n\n'
             b'export fn rate() -> int uses net("api.example.com") {\n    match http.get("http://api.example.com/") {\n        case ok(r) {\n            return r.status;\n        }\n        case err(_) {\n            return 0;\n        }\n    }\n}\n\n'
             b'fn hidden() uses console {\n    print("not exported");\n}\n\n'
             b'export fn each[E: effects](f: fn() uses E) uses E {\n    f();\n}\n',
}
NEWER = {
    "io.vo": b'import std.process;\n\nexport fn rate() -> int uses net("api.example.com") {\n    return 1;\n}\n\n'
             b'export fn tag() -> int uses process("git") {\n    match process.run("git", ["describe"]) {\n        case ok(o) {\n            return o.code;\n        }\n        case err(_) {\n            return 1;\n        }\n    }\n}\n',
}


def group_effects(base, arch, srv):
    for name, files in (("pure", PURE), ("scoped", SCOPED), ("newer", NEWER),
                        ("broken", {"a.vo": b"export fn broken( {\n"}),
                        ("needs", {"a.vo": b"import other.x.y;\n\nexport fn f() -> int {\n    return y();\n}\n"})):
        open(os.path.join(arch, name + ".tar.gz"), "wb").write(tar_bytes(files, top=name))
    d = project(base, "effects")
    code, out, err = add(d, "pure", f"{srv.base}/archive/pure.tar.gz")
    case("effects/a pure package", code == 0 and out.endswith("\n  effects: none\n"), out + err)
    code, out, err = add(d, "lib", f"{srv.base}/archive/scoped.tar.gz")
    want = '  effects: file.read("data/"), file.write("out/"), net("api.example.com")\n'
    case("effects/the union with scopes, in order; private functions and effect parameters left out (SC-004)", code == 0 and out.endswith(want), out + err)
    code, out, err = add(d, "lib", f"{srv.base}/archive/newer.tar.gz")
    want = '  effects: net("api.example.com"), process("git")\n  gains: process("git")\n  loses: file.read("data/"), file.write("out/")\n'
    case("effects/an update shows what it gains and loses (SC-004)", code == 0 and out.endswith(want), out + err)
    code, out, err = vo(d, ["package", "--json", "--ca-file", CA, "main.vo", "lib", f"{srv.base}/archive/scoped.tar.gz"])
    rec = json.loads(out) if out.startswith("{") else {}
    case("effects/the JSON record", rec.get("change") == "updated" and rec.get("url", "").endswith("/archive/scoped.tar.gz")
         and rec.get("effects") == ['file.read("data/")', 'file.write("out/")', 'net("api.example.com")']
         and rec.get("gained") == ['file.read("data/")', 'file.write("out/")'] and rec.get("lost") == ['process("git")'], out + err)
    for name in ("broken", "needs"):
        code, out, err = add(d, name, f"{srv.base}/archive/{name}.tar.gz")
        case(f"effects/a package that does not check ({name}) is added with a note", code == 0 and out.startswith(f"added {name} ")
             and "  effects: could not be read (" in out, out + err)
    elsewhere = os.path.join(base, "elsewhere")
    os.makedirs(elsewhere)
    open(os.path.join(elsewhere, "vo.packages"), "w").write("# vo packages: update with `vo package ENTRY NAME DIRECTORY`\n")
    d2 = project(base, "effects2")
    r = subprocess.run([VO, "package", "--ca-file", CA, os.path.join(d2, "main.vo"), "lib", f"{srv.base}/archive/scoped.tar.gz"],
                       cwd=elsewhere, capture_output=True, text=True)
    want = '  effects: file.read("data/"), file.write("out/"), net("api.example.com")\n'
    case("effects/the same from another working directory with its own manifest", r.returncode == 0 and r.stdout.endswith(want)
         and os.path.isfile(os.path.join(d2, "packages", "lib", "io.vo")) and "lib" not in open(os.path.join(elsewhere, "vo.packages")).read(),
         r.stdout + r.stderr)


def group_fuzz(base, arch, srv):
    rnd = random.Random(7)
    seeds = [tar_bytes({"a.vo": vo_file(1), "d/b.vo": vo_file(2)}, fmt, level, "top")
             for fmt in (tarfile.USTAR_FORMAT, tarfile.GNU_FORMAT, tarfile.PAX_FORMAT) for level in (None, 0, 6, 9)]
    seeds.append(tar_bytes({("x" * 150) + "/a.vo": vo_file(1)}, tarfile.PAX_FORMAT, 6))
    d = project(base, "fuzz")
    open(os.path.join(arch, "f.tar.gz"), "wb").write(seeds[0])
    add(d, "f", f"{srv.base}/archive/f.tar.gz")
    count = 2000 if SLOW == 1 else 100
    odd, changed, crashed = [], 0, 0
    marker = os.path.join(os.path.dirname(d), "outside-marker")
    for i in range(count):
        data = bytearray(rnd.choice(seeds))
        for _ in range(rnd.randint(1, 4)):
            how = rnd.randint(0, 3)
            if how == 0:
                del data[rnd.randint(0, len(data) - 1):]
            elif how == 1 and data:
                data[rnd.randrange(len(data))] ^= 1 << rnd.randint(0, 7)
            elif how == 2 and len(data) > 8:
                at = rnd.randrange(len(data) - 8)
                data[at:at + 8] = bytes(rnd.randrange(256) for _ in range(8))
            elif data:
                other = rnd.choice(seeds)
                data = data[:rnd.randrange(len(data))] + other[rnd.randrange(len(other)):]
            if not data:
                data = bytearray(b"\0")
        open(os.path.join(arch, "f.tar.gz"), "wb").write(bytes(data))
        before = snapshot(d)
        try:
            code, out, err = add(d, "f", f"{srv.base}/archive/f.tar.gz")
        except subprocess.TimeoutExpired:
            code, out, err = -1, "", "timed out"
        if code not in (0, 65):
            crashed += 1
            odd.append((i, code, err[:120]))
        elif code == 65 and snapshot(d) != before:
            changed += 1
            odd.append((i, "changed", ""))
        stray = [n for n in os.listdir(os.path.dirname(d)) if n not in ("fuzz",) and "fuzz" not in n and n.endswith(".vo")]
        if stray or os.path.exists(marker):
            odd.append((i, "outside", stray))
    case(f"fuzz/{count} mutated archives: exit 0 or 65, a failure changes nothing, nothing outside", not odd, str(odd[:5]))


GROUPS = {"corpus": group_corpus, "fetch": group_fetch, "hostile": group_hostile, "quiet": group_quiet,
          "restore": group_restore, "effects": group_effects, "fuzz": group_fuzz}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    tmp = os.path.join(ROOT, "build", "tmp")
    os.makedirs(tmp, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=tmp) as base:
        arch = os.path.join(base, "archives")
        os.makedirs(arch)
        srv = Server(arch)
        try:
            for n in names:
                sub = os.path.join(base, n)
                os.makedirs(sub)
                GROUPS[n](sub, arch, srv)
        finally:
            srv.stop()
    print(f"package_fetch: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
