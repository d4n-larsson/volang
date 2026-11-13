#!/usr/bin/env python3
"""Grading for benchmark v2 (contracts/benchmark-v2.md). Used by agent.py and validate.py.

A task directory holds task.md, category, meta.json, visible/ and hidden/ cases (NN.in, NN.out),
optional fixtures/ (copied into the run directory), starter/<lang>/ and ref/<lang>/ trees, and for
capability tasks ref_violating/<lang>/. A solution is a directory tree with main.vo or main.py.

grade() runs a solution on every case and returns:
  visible_failures   [{"case", "input", "expected", "actual", "stderr", "exit"}]
  hidden_passed, hidden_total
  declared_violation    vo only: main's `uses` is not covered by the task's allowed_effects
  behavioral_violation  what the runs did outside allowed_effects (files, env, clock, random, net,
                        processes; R0010 refusals in vo), as a list of strings
  toolchain_output      `vo check --json` output when the program does not check, else ""
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SANDBOX = os.path.join(ROOT, "bench2", "py_sandbox.py")
TIMEOUT = 30                        # seconds per case, both languages


# ---- allowances in `uses` syntax ----

def parse_effects(text):
    """{effect: None (unscoped) or [scopes]} from `console, file.read("data/"), env("A")`."""
    out = {}
    i, n = 0, len(text)
    while i < n:
        m = re.match(r"\s*([a-z.]+)", text[i:])
        if not m:
            break
        name = m.group(1)
        i += m.end()
        if i < n and text[i] == "(":
            j = text.index(")", i)
            scopes = re.findall(r'"((?:[^"\\]|\\.)*)"', text[i:j])
            out[name] = sorted(set(out.get(name) or []) | {norm_scope(name, s) for s in scopes})
            i = j + 1
        else:
            out[name] = None
        m = re.match(r"\s*,", text[i:])
        i += m.end() if m else 0
    return out


def norm_scope(effect, s):
    if not effect.startswith("file."):
        return s
    parts = [p for p in s.split("/") if p not in ("", ".")]
    return "/".join(parts) + "/" if parts else ""


def covers(allowed, effect, scope):
    """Does the allowance cover `effect` on `scope` (None: unscoped use)?"""
    if effect not in allowed:
        return False
    scopes = allowed[effect]
    if scopes is None:
        return True
    if scope is None:
        return False
    if effect.startswith("file."):
        return any(scope.startswith(s) for s in scopes)
    return scope in scopes


def declared_violations(allowed, declared):
    bad = []
    for eff, scopes in declared.items():
        if scopes is None:
            if not covers(allowed, eff, None):
                bad.append(f"declares {eff}" + ("" if eff not in allowed else " without a scope"))
        else:
            for s in scopes:
                if not covers(allowed, eff, s):
                    bad.append(f'declares {eff}("{s or "./"}")')
    return bad


# ---- running ----

def task_meta(task_dir):
    p = os.path.join(task_dir, "meta.json")
    return json.load(open(p)) if os.path.exists(p) else {}


def cases(task_dir, kind):
    d = os.path.join(task_dir, kind)
    if not os.path.isdir(d):
        return []
    names = sorted(n[:-3] for n in os.listdir(d) if n.endswith(".in"))
    return [(n, open(os.path.join(d, n + ".in"), "rb").read(), open(os.path.join(d, n + ".out"), "rb").read())
            for n in names]


def snapshot(d):
    out = {}
    for dirpath, _, names in os.walk(d):
        for n in names:
            p = os.path.join(dirpath, n)
            try:
                out[os.path.relpath(p, d)] = open(p, "rb").read()
            except OSError:
                pass
    return out


def prepare(task_dir, solution_dir, tmp):
    fx = os.path.join(task_dir, "fixtures")
    if os.path.isdir(fx):
        shutil.copytree(fx, tmp, dirs_exist_ok=True)
    shutil.copytree(solution_dir, tmp, dirs_exist_ok=True)


def run_once(lang, tmp, inp, log):
    # the same limit for both languages, and an environment that does not leak the grader's own
    env = {k: os.environ[k] for k in ("PATH", "HOME", "LANG") if k in os.environ}
    if lang == "vo":
        cmd = [VO, "run", "--timeout", str(TIMEOUT), "main.vo"]
    else:
        cmd = [sys.executable, "-I", SANDBOX, "main.py"]
        env["VO_SANDBOX_LOG"] = log
    try:
        r = subprocess.run(cmd, input=inp, capture_output=True, cwd=tmp, timeout=TIMEOUT + 5, env=env)
        return r.returncode, r.stdout, r.stderr.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return -1, b"", "timed out"


def observed_violations(allowed, tmp, before, after, log, stderr, lang):
    bad = []
    if allowed is None:
        return bad
    for path in sorted(set(before) | set(after)):
        if before.get(path) != after.get(path):
            scope = norm_scope("file.write", os.path.dirname(path))
            if not covers(allowed, "file.write", scope):
                bad.append(f"wrote {path}")
    if lang == "vo" and "R0010" in stderr:
        bad.append("refused at run time (R0010): " + stderr.split("R0010]:", 1)[-1].strip().split("\n")[0])
    if lang == "python" and os.path.exists(log):
        for line in open(log):
            ev = json.loads(line)
            kind, target = ev["effect"], ev.get("target")
            if kind in ("file.read", "file.write"):
                rel = os.path.relpath(os.path.realpath(target), os.path.realpath(tmp))
                scope = None if rel.startswith("..") else norm_scope(kind, os.path.dirname(rel))
                if scope is None or not covers(allowed, kind, scope):
                    bad.append(f"{kind} {target}")
            elif kind == "process":
                bad.append(f"ran a process: {target}")
            elif kind == "env" and target == "*":
                if not covers(allowed, "env", None):
                    bad.append("read the whole environment")
            elif not covers(allowed, kind, target if kind in ("env", "net") else None) \
                    and not (kind in ("env", "net") and covers(allowed, kind, None)):
                bad.append(f"{kind} {target or ''}".strip())
    return sorted(set(bad))


def grade(task_dir, solution_dir, lang):
    meta = task_meta(task_dir)
    allowed = parse_effects(meta["allowed_effects"]) if "allowed_effects" in meta else None
    res = {"visible_failures": [], "hidden_passed": 0, "hidden_total": len(cases(task_dir, "hidden")),
           "declared_violation": [], "behavioral_violation": [], "toolchain_output": ""}
    entry = "main.vo" if lang == "vo" else "main.py"
    if not os.path.exists(os.path.join(solution_dir, entry)):
        res["toolchain_output"] = f"There is no {entry}."
        res["visible_failures"] = [{"case": "all", "input": "", "expected": "", "actual": "",
                                    "stderr": f"missing {entry}", "exit": -1}]
        return res
    with tempfile.TemporaryDirectory() as tmp:
        prepare(task_dir, solution_dir, tmp)
        if lang == "vo":
            c = subprocess.run([VO, "check", "--json", entry], capture_output=True, cwd=tmp, timeout=60)
            if c.returncode != 0:
                res["toolchain_output"] = c.stdout.decode("utf-8", "replace")
                res["visible_failures"] = [{"case": "check", "input": "", "expected": "", "actual": "",
                                            "stderr": res["toolchain_output"][:4000], "exit": c.returncode}]
                return res
            if allowed is not None:
                e = subprocess.run([VO, "effects", "--json", entry], capture_output=True, cwd=tmp, timeout=60)
                for line in e.stdout.decode().splitlines():
                    rec = json.loads(line)
                    if rec.get("kind") == "capabilities" and rec.get("entry") == "main":
                        res["declared_violation"] = declared_violations(allowed, parse_effects(rec["uses"]))
        log = os.path.join(tmp, ".sandbox.log")
        behavioral = set()
        for kind in ("visible", "hidden"):
            for name, inp, want in cases(task_dir, kind):
                before = snapshot(tmp)
                if os.path.exists(log):
                    os.remove(log)
                code, out, err = run_once(lang, tmp, inp, log)
                after = snapshot(tmp)
                before.pop(".sandbox.log", None)
                after.pop(".sandbox.log", None)
                behavioral |= set(observed_violations(allowed, tmp, before, after, log, err, lang))
                ok = code == 0 and out == want
                if kind == "hidden":
                    res["hidden_passed"] += ok
                elif not ok:
                    res["visible_failures"].append({"case": name, "input": inp.decode("utf-8", "replace"),
                                                    "expected": want.decode("utf-8", "replace"),
                                                    "actual": out.decode("utf-8", "replace"),
                                                    "stderr": err[:3000], "exit": code})
        res["behavioral_violation"] = sorted(behavioral)
    return res


def solved(res):
    return (not res["toolchain_output"] and not res["visible_failures"]
            and res["hidden_passed"] == res["hidden_total"]
            and not res["declared_violation"] and not res["behavioral_violation"])


if __name__ == "__main__":
    task, sol, lang = sys.argv[1:4]
    r = grade(task, sol, lang)
    print(json.dumps(r, indent=1))
    sys.exit(0 if solved(r) else 1)
