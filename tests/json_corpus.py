#!/usr/bin/env python3
"""std.json against its corpus (spec 007 SC-003): tests/json/valid/*.json must print their
.expected text and survive parse(write(v)) == v; tests/json/invalid/*.json must return err at
the .expected position. Also times parse_json of a generated 100 KB document (under 2 s)."""
import glob
import json
import os
import random
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
DRIVER = os.path.join(ROOT, "tests", "json", "driver.vo")


def run(data):
    p = subprocess.run([VO, "run", DRIVER], input=data, capture_output=True, timeout=60)
    return p.returncode, p.stdout.decode("utf-8", "replace"), p.stderr.decode("utf-8", "replace")


def main():
    failures = []
    valid = sorted(glob.glob(os.path.join(ROOT, "tests", "json", "valid", "*.json")))
    invalid = sorted(glob.glob(os.path.join(ROOT, "tests", "json", "invalid", "*.json")))
    for f in valid:
        want = open(f[:-5] + ".expected", encoding="utf-8").read()
        code, out, err = run(open(f, "rb").read())
        if code != 0 or out != want:
            failures.append(f"{os.path.basename(f)}: want {want!r}, got {out!r} (exit {code}) {err}")
    for f in invalid:
        at = open(f[:-5] + ".expected").read().strip()  # a position; "not UTF-8 ..." fails before parsing
        want = ("error: invalid json at " if at.startswith("line ") else "error: ") + at + ": "
        code, out, err = run(open(f, "rb").read())
        if code != 0 or not out.startswith(want):
            failures.append(f"{os.path.basename(f)} (invalid): want {want!r}..., got {out!r} (exit {code}) {err}")
    r = random.Random(3)
    doc = json.dumps([{"id": i, "name": "item %d é" % i, "price": r.random() * 100, "tags": ["a", "b"],
                       "ok": i % 2 == 0, "none": None} for i in range(1100)])
    t0 = time.time()
    code, out, _ = run(doc.encode())
    elapsed = time.time() - t0
    if code != 0 or out.startswith("error") or "round trip" in out:
        failures.append(f"the {len(doc)} byte document: {out[:200]!r}")
    for f in failures:
        print("FAIL", f)
    print(f"json_corpus: {len(valid)} valid, {len(invalid)} invalid, {len(failures)} failures; "
          f"{len(doc) // 1024} KB parsed, written and parsed again in {elapsed:.2f} s")
    if elapsed >= 2.0:
        print("FAIL the 100 KB document took 2 s or more")
        return 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
