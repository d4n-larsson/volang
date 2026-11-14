#!/usr/bin/env python3
"""Step cases (spec 006): golden cases that need more than one command, or that produce files.

A step case is a folder under tests/cases/ that contains a file named `steps` (tests/run.sh skips
such folders). Everything in the folder except `expected/` is copied to a temporary directory, and
each line of `steps` runs there, in order:

  vo ARGS... [< FILE]          run $VO_BIN (default bin/vo) with stdin from FILE (default: empty)
  env NAME=VALUE ... vo ARGS... the same, with extra environment variables
  unset NAME ... vo ARGS...     the same, without those variables
  rm PATH | cp A B | write PATH TEXT (TEXT with \\n escapes) | chmod MODE PATH
  keep PATH...                  files the case produces or changes, compared with expected/PATH
  vo~ ARGS...                   like vo, but its stdout is left out of `out` (it may differ between
                                runs, e.g. with a clock seed); `same` then checks the last two were equal

No shell is involved; arguments are split on spaces ("quoted words" may hold spaces).
Afterwards every file in expected/ except `out` must exist with identical bytes, and expected/out
must equal, for each `vo` step: `$ LINE`, its stdout, `--- stderr`, its stderr, `--- exit N`.
Timings (`12ms)`, `"ms":12`) are masked as in run.sh.

  steps.py [--accept] [--repeat N] [PATH...]
"""
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN") or os.path.join(ROOT, "bin", "vo")


def mask(b):
    b = re.sub(rb'"ms":\d+', b'"ms":0', b)
    return re.sub(rb"([ (])\d+ms\)", rb"\g<1>0ms)", b)


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
    try:
        for n in os.listdir(case):
            if n in ("expected", "steps"):
                continue
            src = os.path.join(case, n)
            (shutil.copytree if os.path.isdir(src) else shutil.copy)(src, os.path.join(work, n))
        out = b""
        keep = []
        hidden = []
        for line in open(os.path.join(case, "steps"), encoding="utf-8").read().splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            words = shlex.split(line)
            if words[0] == "keep":
                keep += words[1:]
                continue
            if words[0] == "same":
                ok = len(hidden) >= 2 and hidden[-1] == hidden[-2]
                out += b"same stdout: " + (b"yes" if ok else b"no") + b"\n"
                continue
            if words[0] == "rm":
                p = os.path.join(work, words[1])
                shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
                continue
            if words[0] == "cp":
                shutil.copy(os.path.join(work, words[1]), os.path.join(work, words[2]))
                continue
            if words[0] == "chmod":
                os.chmod(os.path.join(work, words[2]), int(words[1], 8))
                continue
            if words[0] == "write":
                text = line.split(None, 2)[2] if len(line.split(None, 2)) > 2 else ""
                open(os.path.join(work, words[1]), "w", encoding="utf-8").write(text.replace("\\n", "\n"))
                continue
            env = dict(os.environ)
            while words[0] in ("env", "unset"):
                kind = words.pop(0)
                while words and words[0] != "vo" and words[0] not in ("env", "unset"):
                    w = words.pop(0)
                    if kind == "env":
                        k, v = w.split("=", 1)
                        env[k] = v
                    else:
                        env.pop(w, None)
            hide = words[0] == "vo~"
            if hide:
                words[0] = "vo"
            assert words[0] == "vo", f"{case}: unknown step {line!r}"
            stdin = b""
            if len(words) >= 2 and words[-2] == "<":
                stdin = open(os.path.join(work, words[-1]), "rb").read()
                words = words[:-2]
            r = subprocess.run([VO] + words[1:], cwd=work, input=stdin, capture_output=True, env=env, timeout=120)
            if hide:
                hidden.append(r.stdout)
            out += b"$ " + line.encode() + b"\n" + (b"(stdout not shown)\n" if hide else mask(r.stdout)) \
                + b"--- stderr\n" + mask(r.stderr) + b"--- exit " + str(r.returncode).encode() + b"\n"
        got = {"out": out.replace(work.encode() + b"/", b"")}
        for rel in keep:
            p = os.path.join(work, rel)
            got[rel] = mask(open(p, "rb").read()) if os.path.isfile(p) else None
        return got
    finally:
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
