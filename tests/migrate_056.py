#!/usr/bin/env python3
"""Spec 056 (SC-002): programs written before the consistency pass migrate by fixes alone.

For every example (`examples/*/main.vo`, `examples/*/*/main.vo`) and the postgres package, as of commit
8c8db3e (the last commit before spec 056):
  - copy it from git, apply the first fix of every diagnostic (tools/vo_fix.vo, at most 3 rounds, with
    `vo effects --update` when only E0504 is left), and require a clean check;
  - run its tests (with `--replay recordings/tests` when it has them) and replay its recordings, and
    compare with the baseline recorded with the toolchain of that commit (tests/migrate_056/baseline.json),
    except for the intended output changes in CHANGED.

  migrate_056.py --record VO    record the baseline with VO (a toolchain built at 8c8db3e)
  migrate_056.py [--quick]      migrate and compare (--quick: the examples only)
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BEFORE = "8c8db3e"
BASELINE = os.path.join(ROOT, "tests", "migrate_056", "baseline.json")
VO = os.path.join(ROOT, "bin", "vo")

# Intended output changes: "program: what" -> (the task, [(old text, new text)] applied to the baseline's
# output, the exit status now). Anything else must equal the baseline.
CHANGED = {
    # T027: parse_time's text is `invalid time: S; DETAIL`; the example's test compares the old text.
    "examples/expenses: test": ("T027", [
        ("PASS a bad date is an error, not a crash (0ms)\n",
         "FAIL a bad date is an error, not a crash (0ms) [FR-004]\n"
         "  report.vo:160:13: `expect_eq` failed: the values differ.\n"  # spec 058: 10 import lines fewer; 059: 12 more
         '    expected: err(ExpenseError.bad_date(new DateProblem { date: "2026-02-30", reason: "day 30 is not in '
         'that month" }))\n'
         '    actual:   err(ExpenseError.bad_date(new DateProblem { date: "2026-02-30", reason: "invalid time: '
         '2026-02-30; day 30 is not in that month" }))\n'
         '    first difference at .err.bad_date.reason: expected "day 30 is not in that month", actual "invalid '
         'time: 2026-02-30; day 30 is not in that month"\n'),
        ("spec FR-004: 1 test, all passing", "spec FR-004: 1 test, 1 not passing"),
        ("5 passed, 0 failed", "4 passed, 1 failed")], 1),
}


# Spec 059: anonymous functions the fixes leave alone, edited by hand as in the repository (a named setup
# function where nothing gives `ok(...)` its type; `=> e` where the return type is generic but e is not):
# program -> [(file, regex on the fixed text, replacement, text appended to the file)].
HAND_059 = {
    "examples/job-queue": [("main.vo", r"fn\(s: Seat\) -> [\w.]*Worker\[[\w.]+, [\w.]+\] \{\s*return s\.worker;\s*\}", "fn(s) => s.worker", "")],
    "examples/rates": [("main.vo", r"fn\(\) -> result\[convert\.Rates\[uses net\]\] \{\s*return ok\(new convert\.Rates \{ rate: rates\.fetch_rate \}\);\s*\}",
                        "open_rates", "\nfn open_rates() -> result[convert.Rates[uses net]] {\n"
                        "    return ok(new convert.Rates { rate: rates.fetch_rate });\n}\n")],
    "examples/secure-notes": [("main.vo", r"fn\(\) -> result\[store\.Store\] \{\s*return ok\(store\.new_store\(\)\);\s*\}",
                               "open_store", "\nfn open_store() -> result[store.Store] {\n    return ok(store.new_store());\n}\n")],
}


def hand_059(name, where):
    """Applies HAND_059's edits of a program; 1 when one applied."""
    done = 0
    for f, pattern, new, tail in HAND_059.get(name, []):
        path = os.path.join(where, f)
        text, n = re.subn(pattern, new, open(path, encoding="utf-8").read())
        if n:
            open(path, "w", encoding="utf-8").write(text + tail)
            done = 1
    return done


def changed(key, want):
    """The baseline's output with the intended changes of CHANGED."""
    if key not in CHANGED:
        return want
    _, edits, code = CHANGED[key]
    out = want["out"]
    for old, new in edits:
        assert old in out, (key, old)
        out = out.replace(old, new)
    return {"out": out, "code": code}


def norm(text):
    text = re.sub(r"\b\d+ms([),])", r"0ms\1", text)
    return re.sub(r"\(\d+ cases, \d+ms\)", "(N cases, 0ms)", text)


def programs():
    """(name, directory in the repository, kind): kind is 'example' or 'package'."""
    out = []
    listed = subprocess.run(["git", "ls-tree", "-r", "--name-only", BEFORE, "examples", "packages"], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.split()
    for p in sorted(listed):
        parts = p.split("/")
        if parts[0] == "examples" and parts[-1] == "main.vo" and len(parts) in (3, 4):
            out.append((os.path.dirname(p), os.path.dirname(p), "example"))
    out.append(("packages/postgres", "packages/postgres", "package"))
    return out


def checkout(tmp):
    """The repository's examples and packages as of BEFORE, under tmp."""
    tar = subprocess.run(["git", "archive", BEFORE, "examples", "packages"], cwd=ROOT, capture_output=True,
                         check=True).stdout
    subprocess.run(["tar", "-x", "-C", tmp], input=tar, check=True)


def run(vo, args, cwd):
    r = subprocess.run([vo] + args, cwd=cwd, capture_output=True, timeout=600)
    return {"out": norm(r.stdout.decode("utf-8", "replace")), "code": r.returncode}


def host(vo, pkg_dir):
    """A project with the package pinned as `postgres` (as tests/postgres.py builds it); its directory."""
    d = pkg_dir + "-host"
    if not os.path.isdir(d):
        os.makedirs(os.path.join(d, "pkg"))
        shutil.copytree(pkg_dir, os.path.join(d, "pkg", "postgres"))
        with open(os.path.join(d, "main.vo"), "w") as f:
            f.write("fn main() {\n}\n")
    subprocess.run([vo, "package", "main.vo", "postgres", "pkg/postgres"], cwd=d, capture_output=True, check=True)
    return d


def observe(vo, d, kind):
    """What a program does: its tests' output and its recordings' replays."""
    seen = {}
    if kind == "example":
        replay = os.path.isdir(os.path.join(d, "recordings", "tests"))
        seen["test"] = run(vo, ["test"] + (["--replay", "recordings/tests"] if replay else []) + ["main.vo"], d)
        rec = os.path.join(d, "recordings")
        for f in sorted(os.listdir(rec)) if os.path.isdir(rec) else []:
            if f.endswith(".jsonl"):
                seen["replay " + f] = run(vo, ["replay", "recordings/" + f], d)
    else:
        h = host(vo, d)
        for f in sorted(os.listdir(d)):
            if f.endswith(".vo"):
                seen["test " + f] = run(vo, ["test", "pkg/postgres/" + f], h)
    return seen


def record(vo):
    tmp = tempfile.mkdtemp(prefix="vo-m056-")
    try:
        checkout(tmp)
        base = {name: observe(vo, os.path.join(tmp, d), kind) for name, d, kind in programs()}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(os.path.dirname(BASELINE), exist_ok=True)
    with open(BASELINE, "w") as f:
        json.dump(base, f, indent=1, sort_keys=True)
        f.write("\n")
    print(f"migrate_056: recorded {len(base)} programs")


def verify(quick):
    sys.path.insert(0, os.path.join(ROOT, "tests"))
    from apply_fixes import diagnostics, vo_fix  # noqa: E402
    base = json.load(open(BASELINE))
    problems, used = [], {}
    tmp = tempfile.mkdtemp(prefix="vo-m056-")
    try:
        checkout(tmp)
        for name, d, kind in programs():
            if quick and kind == "package":
                continue
            where = os.path.join(tmp, d)
            if kind == "example":
                paths = [os.path.join(where, "main.vo")]
            else:  # the package's modules, fixed inside its host project, then the host's copy is the package
                h = host(VO, where)
                paths = [os.path.join(h, "pkg", "postgres", f) for f in sorted(os.listdir(where)) if f.endswith(".vo")]
            rows = sorted({m.group(1) for path in paths for r in diagnostics(path) if r["code"] == "E0214"
                           for m in [re.match(r"`(std\.[\w.]+)`", r["message"])] if m})
            fixed, left = vo_fix(paths)
            for _ in range(4) if kind == "package" else ():  # the fixes changed the package: pin it again and
                subprocess.run([VO, "package", "main.vo", "postgres", "pkg/postgres"], cwd=h, capture_output=True)
                fixed2, left = vo_fix(paths)                 # fix again (spec 059: E0413 follows E0342)
                fixed |= fixed2
                if not fixed2:
                    break
                shutil.rmtree(where)
                shutil.copytree(os.path.join(h, "pkg", "postgres"), where)
            if hand_059(name, where):
                fixed2, left = vo_fix(paths)
                fixed |= fixed2 | {"hand edit (spec 059)"}
            used[name] = sorted(fixed) + ([f"rows {', '.join(rows)}"] if rows else [])
            if left:
                problems.append(f"{name}: not clean after the fixes: {left[:3]}")
                continue
            now = observe(VO, where, kind)
            for what, want in base[name].items():
                want = {"out": norm(want["out"]), "code": want["code"]}
                want = changed(f"{name}: {what}", want)
                if now.get(what) != want:
                    problems.append(f"{name}: {what} changed:\n--- before\n{want['out']}(exit {want['code']})\n"
                                    f"--- after\n{now.get(what, {}).get('out')}(exit {now.get(what, {}).get('code')})")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for name, codes in used.items():
        print(f"{name}: {', '.join(codes) or 'no fixes needed'}")
    for p in problems:
        print("FAIL " + p)
    print(f"migrate_056: {len(used)} programs, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--record":
        record(os.path.abspath(sys.argv[2]))
    else:
        sys.exit(verify("--quick" in sys.argv))
