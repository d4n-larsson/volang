#!/usr/bin/env python3
"""Spec 061 (research R1, R8): the git pack reader against packs written by hand.

  package_git.py [--count N] [--seed S]   N generated packs (default 200), then the hostile ones
  package_git.py --fuzz N                 N mutated packs: no crash, nothing written outside the target

Each pack holds one commit of a generated tree (.vo files and others, nested folders), its objects
written plain, as OFS_DELTA or as REF_DELTA (chains up to depth 5, a base after its delta), at every
compression level, sometimes behind an annotated tag. `vo package --unpack-pack PACK ID DIR [FOLDER]`
(a test entry, not in the usage text) must write exactly the tree's .vo files (and the root
vo.packages) under FOLDER. A hostile pack must fail with E0706 and write nothing.
"""
import hashlib
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
COMMIT, TREE, BLOB, TAG, OFS, REF = 1, 2, 3, 4, 6, 7
NAMES = {COMMIT: b"commit", TREE: b"tree", BLOB: b"blob", TAG: b"tag"}


def oid(kind, data):
    return hashlib.sha1(NAMES[kind] + b" %d\0" % len(data) + data).digest()


def varint(n):
    out = b""
    while True:
        b = n & 0x7f
        n >>= 7
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def delta(base, target, r):
    """A git delta turning base into target: copies of a common prefix and suffix around an insert."""
    p = 0
    while p < min(len(base), len(target)) and base[p] == target[p]:
        p += 1
    s = 0
    while s < min(len(base), len(target)) - p and base[-1 - s] == target[-1 - s]:
        s += 1
    out = varint(len(base)) + varint(len(target))

    def copy(off, n):
        o = b""
        while n:
            k = min(n, r.choice([0x10000, 0xffff, 300, n]))
            cmd, args = 0x80, b""
            for i in range(4):
                if (off >> (8 * i)) & 0xff:
                    cmd |= 1 << i
                    args += bytes([(off >> (8 * i)) & 0xff])
            if k != 0x10000:
                for i in range(3):
                    if (k >> (8 * i)) & 0xff:
                        cmd |= 0x10 << i
                        args += bytes([(k >> (8 * i)) & 0xff])
            o += bytes([cmd]) + args
            off += k
            n -= k
        return o
    out += copy(0, p)
    mid = target[p:len(target) - s]
    for i in range(0, len(mid), 127):
        out += bytes([len(mid[i:i + 127])]) + mid[i:i + 127]
    out += copy(len(base) - s, s)
    return out


def entry_header(kind, size):
    b = (kind << 4) | (size & 15)
    size >>= 4
    out = b""
    while size:
        out += bytes([b | 0x80])
        b = size & 0x7f
        size >>= 7
    return out + bytes([b])


def ofs_bytes(n):
    out = bytes([n & 0x7f])
    n >>= 7
    while n:
        n -= 1
        out = bytes([0x80 | (n & 0x7f)]) + out
        n >>= 7
    return out


class Pack:
    def __init__(self, r):
        self.r, self.objs = r, []  # (kind, data, id)

    def add(self, kind, data):
        i = oid(kind, data)
        if all(o[2] != i for o in self.objs):
            self.objs.append((kind, data, i))
        return i

    def write(self, deltas=True, version=2, count=None):
        r, order = self.r, list(self.objs)
        r.shuffle(order)
        body, offs, depth = b"", {}, {}
        pending = []
        for idx, (kind, data, i) in enumerate(order):
            level = r.choice([0, 1, 6, 9])
            at = 12 + len(body)
            earlier = [o for o in order[:idx] if o[0] == kind and depth.get(o[2], 0) < 5]
            later = [o for o in order[idx + 1:] if o[0] == kind]
            how = r.choice(["plain", "ofs", "ofs", "ref", "ref", "ref-later"]) if deltas else "plain"
            if how == "ofs" and earlier:
                b = r.choice(earlier)
                d = delta(b[1], data, r)
                body += entry_header(OFS, len(d)) + ofs_bytes(at - offs[b[2]]) + zlib.compress(d, level)
                depth[i] = depth.get(b[2], 0) + 1
            elif how == "ref" and earlier:
                b = r.choice(earlier)
                d = delta(b[1], data, r)
                body += entry_header(REF, len(d)) + b[2] + zlib.compress(d, level)
                depth[i] = depth.get(b[2], 0) + 1
            elif how == "ref-later" and later and not any(p == i for p in pending):
                b = r.choice(later)  # the base comes after its delta: it must be written plain
                pending.append(b[2])
                d = delta(b[1], data, r)
                body += entry_header(REF, len(d)) + b[2] + zlib.compress(d, level)
                depth[i] = 1
            else:
                body += entry_header(kind, len(data)) + zlib.compress(data, level)
            offs[i] = at
            if i in pending:  # was promised plain: rewrite it so
                pass
        # a base promised to a "ref-later" delta must not itself be a delta of that delta (a cycle):
        # rebuild without deltas for those and try again when one was written as a delta
        if any(depth.get(p, 0) for p in pending):
            return self.write(deltas=False, version=version, count=count)
        head = b"PACK" + struct.pack(">II", version, len(order) if count is None else count)
        return head + body + hashlib.sha1(head + body).digest()


def tree_bytes(entries):
    """entries: (mode, name, id), sorted as git does (directories as name + "/")."""
    key = lambda e: e[1] + (b"/" if e[0] == b"40000" else b"")  # noqa: E731
    return b"".join(m + b" " + n + b"\0" + i for m, n, i in sorted(entries, key=key))


def gen_tree(r, depth=0):
    """A nested dict: name -> bytes (a file) or dict (a folder)."""
    t = {}
    for k in range(r.randint(1, 5)):
        ext = r.choice([".vo", ".vo", ".md", ".txt", ""])
        body = bytes(r.choice(b"abcdefgh \n{}();") for _ in range(r.choice([0, 3, 40, 400, 3000])))
        if r.random() < 0.3 and t:  # a near copy, so deltas have something to share
            body = r.choice([v for v in t.values() if isinstance(v, bytes)] or [body]) + b"\n# more\n"
        t[f"f{k}{ext}"] = body
    if depth < 3:
        for k in range(r.randint(0, 2)):
            t[f"d{k}"] = gen_tree(r, depth + 1)
    if depth == 0 or r.random() < 0.2:
        t["vo.packages"] = b"# vo packages\n"
    return t


def add_tree(pack, t):
    entries = []
    for name, v in t.items():
        if isinstance(v, dict):
            entries.append((b"40000", name.encode(), add_tree(pack, v)))
        else:
            entries.append((b"100644", name.encode(), pack.add(BLOB, v)))
    return pack.add(TREE, tree_bytes(entries))


def commit_bytes(tree):
    return b"tree " + tree.hex().encode() + b"\nauthor a <a@b> 0 +0000\ncommitter a <a@b> 0 +0000\n\nm\n"


def expected(t, folder):
    for part in [p for p in folder.split("/") if p]:
        t = t.get(part, {})
    out = {}

    def walk(d, prefix):
        for name, v in d.items():
            if isinstance(v, dict):
                walk(v, prefix + name + "/")
            elif name.endswith(".vo") and len(name) > 3 or (prefix == "" and name == "vo.packages"):
                out[prefix + name] = v
    walk(t, "")
    return out


def unpack(work, pack, want, folder=""):
    p = os.path.join(work, "p.pack")
    open(p, "wb").write(pack)
    out = os.path.join(work, "out")
    shutil.rmtree(out, ignore_errors=True)
    args = [VO, "package", "--unpack-pack", p, want.hex(), out] + ([folder] if folder else [])
    r = subprocess.run(args, capture_output=True, timeout=120)
    got = {}
    for d, _, files in os.walk(out):
        for f in files:
            full = os.path.join(d, f)
            got[os.path.relpath(full, out)] = open(full, "rb").read()
    return r, got


def one(r, work, n):
    t = gen_tree(r)
    pack = Pack(r)
    want = pack.add(COMMIT, commit_bytes(add_tree(pack, t)))
    if n % 4 == 0:
        want = pack.add(TAG, b"object " + want.hex().encode() + b"\ntype commit\ntag v1\ntagger a <a@b> 0 +0000\n\nt\n")
    folders = [""] + [k for k, v in t.items() if isinstance(v, dict)]
    folder = r.choice(folders)
    want_files = expected(t, folder)
    res, got = unpack(work, pack.write(), want, folder)
    if not any(k.endswith(".vo") for k in want_files):  # a lone vo.packages is no package
        return None if res.returncode == 65 and not got else f"pack {n}: no .vo files, status {res.returncode}, {sorted(got)}"
    if res.returncode != 0 or got != want_files:
        return f"pack {n} (folder {folder!r}): status {res.returncode} {res.stdout.decode()[-300:]} " \
               f"missing {sorted(set(want_files) - set(got))} extra {sorted(set(got) - set(want_files))} " \
               f"differ {sorted(k for k in got if k in want_files and got[k] != want_files[k])}"
    return None


def hostile(work):
    """(label, pack bytes, wanted id): each must fail with E0706 and write nothing."""
    r = random.Random(7)

    def base(mut=None, tree_extra=None, version=2, count=None, blob=b"fn main() {\n}\n"):
        p = Pack(r)
        b = p.add(BLOB, blob)
        entries = [(b"100644", b"main.vo", b)] + (tree_extra(p) if tree_extra else [])
        want = p.add(COMMIT, commit_bytes(p.add(TREE, tree_bytes(entries))))
        data = p.write(deltas=False, version=version, count=count)
        return (mut(data) if mut else data), want

    def resum(body):
        return body + hashlib.sha1(body).digest()
    cases = [
        ("bad magic", *base(lambda d: resum(b"KCAP" + d[4:-20]))),
        ("version 3", *base(version=3)),
        ("object count too high", *base(count=9)),
        ("a truncated stream", *base(lambda d: resum(d[:len(d) // 2]))),
        ("a wrong trailer", *base(lambda d: d[:-1] + bytes([d[-1] ^ 1]))),
        ("a link in the folder", *base(tree_extra=lambda p: [(b"120000", b"l.vo", p.add(BLOB, b"main.vo"))])),
        ("a submodule in the folder", *base(tree_extra=lambda p: [(b"160000", b"sub", b"\x11" * 20)])),
        ("a name with ..", *base(tree_extra=lambda p: [(b"100644", b"..", p.add(BLOB, b"x"))])),
        ("a name with /", *base(tree_extra=lambda p: [(b"100644", b"a/b.vo", p.add(BLOB, b"x"))])),
        ("a missing blob", *base(tree_extra=lambda p: [(b"100644", b"gone.vo", b"\x22" * 20)])),
        ("a missing tree", *base(tree_extra=lambda p: [(b"40000", b"dir", b"\x33" * 20)])),
    ]
    data, want = base()
    cases.append(("a commit that is not in the pack", data, b"\x44" * 20))
    # deltas written by hand
    blob = b"fn main() {\n}\n"
    tree = tree_bytes([(b"100644", b"main.vo", oid(BLOB, blob))])
    commit = commit_bytes(oid(TREE, tree))

    def raw(objs):
        body = b"".join(objs)
        head = b"PACK" + struct.pack(">II", 2, len(objs))
        return resum(head + body)

    def plain(kind, data):
        return entry_header(kind, len(data)) + zlib.compress(data)
    good = [plain(COMMIT, commit), plain(TREE, tree)]
    d = varint(5) + varint(len(blob)) + bytes([len(blob)]) + blob
    cases.append(("a delta with a missing base", raw(good + [entry_header(REF, len(d)) + b"\x55" * 20 + zlib.compress(d)]), oid(COMMIT, commit)))
    other = b"hello"
    d = varint(len(other)) + varint(len(blob) + 3) + bytes([len(blob)]) + blob
    cases.append(("a delta whose result size is wrong", raw(good + [plain(BLOB, other), entry_header(REF, len(d)) + oid(BLOB, other) + zlib.compress(d)]), oid(COMMIT, commit)))
    d = varint(len(other)) + varint(9) + bytes([0x91, 2, 9])  # copy 9 bytes from offset 2 of a 5-byte base
    cases.append(("a delta copying past its base", raw(good + [plain(BLOB, other), entry_header(REF, len(d)) + oid(BLOB, other) + zlib.compress(d)]), oid(COMMIT, commit)))
    d = varint(len(blob)) + varint(len(blob)) + bytes([0x90, len(blob)])
    cases.append(("a delta that is its own base", raw(good + [entry_header(REF, len(d)) + oid(BLOB, blob) + zlib.compress(d)]), oid(COMMIT, commit)))
    big = b"\0" * (65 << 20)
    cases.append(("more than 64 MiB inflated", raw(good + [plain(BLOB, blob), plain(BLOB, big)]), oid(COMMIT, commit)))
    return cases


def main():
    args = sys.argv[1:]
    count = int(args[args.index("--count") + 1]) if "--count" in args else 200
    seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 1
    work = tempfile.mkdtemp(prefix="vo-pack-")
    problems = []
    try:
        if "--fuzz" in args:
            n = int(args[args.index("--fuzz") + 1])
            r = random.Random(seed)
            crashes = 0
            for k in range(n):
                t = gen_tree(r)
                pack = Pack(r)
                want = pack.add(COMMIT, commit_bytes(add_tree(pack, t)))
                data = bytearray(pack.write())
                for _ in range(r.randint(1, 4)):
                    data[r.randrange(len(data))] = r.randrange(256)
                if r.random() < 0.5:  # a valid trailer, so the damage reaches the reader
                    data = bytearray(bytes(data[:-20]) + hashlib.sha1(bytes(data[:-20])).digest())
                res, got = unpack(work, bytes(data), want)
                if res.returncode not in (0, 65) or sorted(os.listdir(work)) not in (["out", "p.pack"], ["p.pack"]):
                    crashes += 1
                    problems.append(f"fuzz {k}: status {res.returncode}, {sorted(os.listdir(work))}")
            print(f"package_git --fuzz: {n} mutated packs, {crashes} problems")
            return 1 if problems else 0
        r = random.Random(seed)
        for n in range(count):
            p = one(r, work, n)
            if p:
                problems.append(p)
        cases = hostile(work)
        for label, data, want in cases:
            res, got = unpack(work, data, want)
            if res.returncode != 65 or b"E0706" not in res.stdout + res.stderr or got:
                problems.append(f"hostile ({label}): status {res.returncode}, files {sorted(got)}, {(res.stdout + res.stderr).decode()[-200:]}")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for p in problems[:20]:
        print("FAIL", p)
    print(f"package_git: {count} packs, {len(cases)} hostile, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
