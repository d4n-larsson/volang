#!/usr/bin/env python3
"""Randomized scope enforcement (SC-003a, T012).

Builds a tree with an allowed directory `in/`, a secret outside it, and symlinks that point out
of and inside `in/`. Each iteration reads one random path in a program granted
`file.read("in/")`; it must be refused with R0010 exactly when the path's real location lies
outside `in/`. The same is done for env names against `env("A_OK")`. Spec 013 (L1): the same path
as a literal must be E0503 at `vo check` exactly when it lies outside.
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
PARTS = ["in", "sub", "..", ".", "link_out", "link_in", "f", "secret", "nope", ""]


def run(src, cwd, env=None, command="run"):
    path = os.path.join(cwd, "prog.vo")
    with open(path, "w") as f:
        f.write(src)
    r = subprocess.run([VO, command, "prog.vo"], cwd=cwd, capture_output=True, timeout=60,
                       env=env)
    return r.returncode, (r.stderr + r.stdout).decode("utf-8", "replace")


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=300)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    rng = random.Random(a.seed)
    bad = 0
    counts = {"inside": 0, "outside": 0}
    with tempfile.TemporaryDirectory() as tmp:
        tmp = os.path.realpath(tmp)
        os.makedirs(os.path.join(tmp, "in", "sub"))
        os.makedirs(os.path.join(tmp, "secret"))
        for p in ("in/sub/f", "secret/f", "in/f"):
            open(os.path.join(tmp, p), "w").write("x")
        os.symlink("../secret", os.path.join(tmp, "in", "link_out"))
        os.symlink("sub", os.path.join(tmp, "in", "link_in"))
        allowed = os.path.join(tmp, "in")
        for i in range(a.iterations):
            n = rng.randint(1, 5)
            path = "/".join(rng.choice(PARTS) for _ in range(n))
            if rng.random() < 0.15:
                path = tmp + "/" + path
            if rng.random() < 0.5 and not path.startswith("/"):
                path = "in/" + path
            if not path:
                path = "."
            real = os.path.realpath(os.path.join(tmp, path))
            inside = real == allowed or real.startswith(allowed + "/")
            counts["inside" if inside else "outside"] += 1
            body = ('        case ok(t) {\n            print("read");\n        }\n'
                    '        case err(e) {\n            print("err");\n        }\n    }\n}\n')
            src = ('fn main() uses console, file.read("in/") {\n'
                   f'    let p = {vo_str(path)};\n    match read_file(p) {{\n' + body)
            code, err = run(src, tmp)
            denied = "R0010" in err
            if denied == inside or (not denied and code != 0):
                bad += 1
                print(f"MISMATCH path={path!r} real={real} inside={inside} exit={code} {err[:200]}")
            src = ('fn main() uses console, file.read("in/") {\n'
                   f'    match read_file({vo_str(path)}) {{\n' + body)
            code, err = run(src, tmp, command="check")
            if ("E0503" in err) == inside or (inside and code != 0):
                bad += 1
                print(f"MISMATCH (check) path={path!r} real={real} inside={inside} exit={code} {err[:200]}")
        names = ["A_OK", "A_NO", "PATH", "HOME", "A_OK2", "A", "a_ok"]
        env = dict(os.environ, A_OK="1", A_NO="1", A_OK2="1")
        for i in range(max(10, a.iterations // 10)):
            name = rng.choice(names)
            split = rng.randint(0, len(name))
            src = ('fn main() uses console, env("A_OK") {\n'
                   f'    match env({vo_str(name[:split])} + {vo_str(name[split:])}) {{\n'
                   '        case ok(v) {\n            print("set");\n        }\n'
                   '        case err(e) {\n            print(e);\n        }\n    }\n}\n')
            code, err = run(src, tmp, env)
            denied = "R0010" in err
            if denied == (name == "A_OK"):
                bad += 1
                print(f"MISMATCH env={name} exit={code} {err[:200]}")
    print(f"scope_fuzz: {a.iterations} paths ({counts['inside']} inside, {counts['outside']} outside), "
          f"{max(10, a.iterations // 10)} env names, {bad} mismatches")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
