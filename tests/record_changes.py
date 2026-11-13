#!/usr/bin/env python3
"""Effects-record change sets (US2, SC-004, T025).

Each tests/effect_changes/NN/ has before/ (a project with main.vo), after/ (changed files, laid
over before/; a file containing only `DELETE` removes it) and expected.txt: one
`<change> <file>:<function>` per line, sorted (empty: nothing changes), or the single line
`check-error` (the edit leaves a check error: `vo effects --update` must exit 65 and leave the
record untouched). A REQUIRES file names a feature the set needs (skipped while it is missing).

For each set: record before/ with `vo effects --update`; overlay after/; `vo check --json` must
report exactly the expected E0504/W0502/W0503 changes and nothing else; `vo effects --update
--json` must print the same changes; the record lines whose effects changed must be exactly
those functions (plus a rename's old name); `vo check` must then pass.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SETS = os.path.join(ROOT, "tests", "effect_changes")
CODES = {"E0504", "W0502", "W0503"}


def vo(args, cwd):
    r = subprocess.run([VO] + args, cwd=cwd, capture_output=True, timeout=120)
    recs = [json.loads(l) for l in r.stdout.decode("utf-8", "replace").splitlines() if l.startswith("{")]
    return r.returncode, recs


def record_effects(path):
    """{"file:function": effects text} from a vo.effects file (pure entries are left out)."""
    out = {}
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#") or line.endswith(" pure"):
            continue
        key, _, effects = line.partition(" uses ")
        out[key] = effects
    return out


def change_of(diag):
    """`<change> <file>:<function>` for a record diagnostic."""
    msg = diag["message"]
    name = msg.split("`")[1]
    if diag["code"] == "E0504":
        kind = "grew"
    elif diag["code"] == "W0503":
        kind = "renamed"
    else:
        kind = "shrank"
    return kind, name


def run_set(d, problems):
    name = os.path.basename(d)
    expected = open(os.path.join(d, "expected.txt")).read().split("\n")
    expected = sorted(l for l in expected if l.strip())
    with tempfile.TemporaryDirectory() as tmp:
        proj = os.path.join(tmp, "p")
        shutil.copytree(os.path.join(d, "before"), proj)
        rc, _ = vo(["effects", "--update", "main.vo"], proj)
        rc2, _ = vo(["check", "main.vo"], proj)
        if rc != 0 or rc2 != 0:
            problems.append(f"{name}: before/ does not record and check cleanly ({rc}, {rc2})")
            return
        old = record_effects(os.path.join(proj, "vo.effects"))
        old_text = open(os.path.join(proj, "vo.effects"), "rb").read()
        after = os.path.join(d, "after")
        for dirpath, _, names in os.walk(after):
            for n in names:
                src = os.path.join(dirpath, n)
                dst = os.path.join(proj, os.path.relpath(src, after))
                if open(src).read().strip() == "DELETE":
                    os.remove(dst)
                else:
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copy(src, dst)
        if expected == ["check-error"]:
            rc, _ = vo(["check", "main.vo"], proj)
            rc2, _ = vo(["effects", "--update", "main.vo"], proj)
            if rc != 65 or rc2 != 65 or open(os.path.join(proj, "vo.effects"), "rb").read() != old_text:
                problems.append(f"{name}: a check error must stop `effects --update` (exits {rc}, {rc2})")
            return
        rc, recs = vo(["check", "--json", "main.vo"], proj)
        diags = [r for r in recs if r.get("kind") == "diagnostic"]
        others = [r["code"] + " " + r["message"] for r in diags if r["code"] not in CODES]
        if others:
            problems.append(f"{name}: unexpected diagnostics {others}")
        _, recs = vo(["effects", "--update", "--json", "main.vo"], proj)
        changes = [r for r in recs if r.get("kind") == "effect_change"]
        got = sorted(f"{c['change']} {c['file']}:{c['function']}" for c in changes)
        if got != expected:
            problems.append(f"{name}: changes {got}, expected {expected}")
        # the check reported the same functions, with the right kind of diagnostic
        kinds = {"added": "grew", "widened": "grew", "removed": "shrank", "narrowed": "shrank",
                 "renamed": "renamed"}
        want = sorted((kinds[c["change"]], c["function"]) for c in changes)
        have = sorted(change_of(r) for r in diags if r["code"] in CODES)
        if want != have:
            problems.append(f"{name}: check reported {have}, update reported {want}")
        new = record_effects(os.path.join(proj, "vo.effects"))
        changed = {k for k in set(old) | set(new) if old.get(k) != new.get(k)}
        listed = {f"{c['file']}:{c['function']}" for c in changes}
        listed |= {f"{c['file']}:{c['old_function']}" for c in changes if "old_function" in c}
        if changed != listed:
            problems.append(f"{name}: record lines changed for {sorted(changed)}, listed {sorted(listed)}")
        rc, recs = vo(["check", "--json", "main.vo"], proj)
        left = [r["code"] for r in recs if r.get("kind") == "diagnostic"]
        if rc != 0 or left:
            problems.append(f"{name}: after --update, check still reports {left}")


def main():
    problems, ran, skipped, altering = [], 0, [], 0
    for name in sorted(os.listdir(SETS)):
        d = os.path.join(SETS, name)
        if not os.path.isdir(d):
            continue
        req = os.path.join(d, "REQUIRES")     # a golden area that must exist first, e.g. `func`
        if os.path.exists(req) and not os.listdir(os.path.join(ROOT, "tests", "cases", open(req).read().strip())):
            skipped.append(name)
            continue
        run_set(d, problems)
        ran += 1
        lines = [l for l in open(os.path.join(d, "expected.txt")).read().split("\n") if l.strip()]
        if lines and lines != ["check-error"]:
            altering += 1
    for p in problems:
        print("FAIL " + p)
    print(f"record_changes: {ran} sets ({altering} alter effects), {len(problems)} problems"
          + (f", skipped until their feature exists: {skipped}" if skipped else ""))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
