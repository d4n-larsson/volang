#!/usr/bin/env python3
"""Memory bounds (spec 003 SC-001, SC-005, FR-009). Usage: tests/gc_bounds.py [--repeat]

- Each workload in tests/gc/workloads prints its .out, allocates at least 1 GiB and peaks under
  64 MiB of live data (`run --mem-stats --json`).
- Out of memory (US3): grow_forever.vo at --max-memory 16 gives identical R0006 records 20 times,
  with limit <= in_use <= 1.1 x limit, and fails later at 32 MiB.
- Test independence (US2): each test's memory record is the same whatever the test order.
- --repeat: the memory records of tests/gc/edge and tests/cases/run are identical across 5 runs.
"""
import concurrent.futures as cf
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
GC = os.path.join(ROOT, "tests", "gc")
MIB, GIB = 1 << 20, 1 << 30


def vo(*args, stdin=b""):
    return subprocess.run([VO, *args], input=stdin, capture_output=True, timeout=600)


def records(text):
    return [json.loads(line) for line in text.decode().splitlines() if line.startswith("{")]


def workload(name):
    p = os.path.join(GC, "workloads", name + ".vo")
    r = vo("run", "--timeout", "120", "--mem-stats", "--json", p)
    mem = [x for x in records(r.stderr) if x.get("kind") == "memory"]
    problems = []
    if r.returncode != 0:
        problems.append(f"exit {r.returncode}: {r.stderr.decode()[:300]}")
    if r.stdout != open(p[:-3] + ".out", "rb").read():
        problems.append(f"stdout {r.stdout[:200]!r}")
    if len(mem) != 1:
        problems.append("no memory record")
        return name, problems, ""
    m = mem[0]
    if m["peak_live"] >= 64 * MIB:
        problems.append(f"peak_live {m['peak_live']} >= 64 MiB")
    if m["allocated"] < GIB:
        problems.append(f"allocated {m['allocated']} < 1 GiB")
    return name, problems, f"peak {m['peak_live'] / MIB:.1f} MiB, allocated {m['allocated'] / GIB:.2f} GiB, {m['collections']} collections"


def check_workloads():
    names = sorted(n[:-3] for n in os.listdir(os.path.join(GC, "workloads")) if n.endswith(".vo"))
    bad = 0
    with cf.ThreadPoolExecutor(len(names)) as ex:
        for name, problems, info in ex.map(workload, names):
            print(("ok   " if not problems else "FAIL ") + f"{name}: {info}")
            for p in problems:
                print("     " + p)
            bad += bool(problems)
    return bad


def oom_record(limit):
    r = vo("run", "--max-memory", str(limit), "--json", os.path.join(GC, "edge", "grow_forever.vo"))
    errs = [x for x in records(r.stderr) if x.get("kind") == "runtime_error"]
    return r.returncode, (errs[0] if errs else None)


def check_oom():
    path = os.path.join(GC, "edge", "grow_forever.vo")
    if not os.path.exists(path):
        print("skip out-of-memory checks (grow_forever.vo not written yet)")
        return 0
    with cf.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(lambda _: oom_record(16), range(20)))
    code, rec = runs[0]
    problems = []
    if code != 70 or not rec or rec.get("code") != "R0006":
        problems.append(f"expected R0006 with exit 70, got exit {code}: {rec}")
    elif any(r != runs[0] for r in runs):
        problems.append("the 20 runs differ")
    elif not (rec["limit"] <= rec["in_use"] <= 1.1 * rec["limit"]):
        problems.append(f"in_use {rec['in_use']} not within 10% above limit {rec['limit']}")
    else:
        code32, rec32 = oom_record(32)
        if not rec32 or rec32.get("code") != "R0006" or rec32["in_use"] <= rec["in_use"]:
            problems.append(f"--max-memory 32 does not fail later than 16: {rec32}")
    print(("ok   " if not problems else "FAIL ") + "out of memory: 20 identical R0006 reports at 16 MiB"
          + (f" (in_use {rec['in_use'] / MIB:.2f} MiB)" if rec and "in_use" in rec else ""))
    for p in problems:
        print("     " + p)
    return bool(problems)


def test_memory(path):
    r = vo("test", "--mem-stats", "--json", path)
    return {x["test"]: x for x in records(r.stdout) if x.get("kind") == "memory"}


def check_test_order():
    path = os.path.join(GC, "edge", "test_order.vo")
    if not os.path.exists(path):
        print("skip test-order check (test_order.vo not written yet)")
        return 0
    src = open(path).read()
    blocks = src.split("\ntest ")
    rev = blocks[0] + "".join("\ntest " + b.rstrip("\n") + "\n" for b in reversed(blocks[1:]))
    # a much longer path as well: a test's memory must not follow where its file is (the CI checkout's
    # longer path once made a report's buffer grow inside a test's figures)
    tmp = os.path.join(ROOT, "build", "tmp-gc-order", "p" * 80)
    os.makedirs(tmp, exist_ok=True)
    rpath = os.path.join(tmp, "test_order.vo")
    open(rpath, "w").write(rev)
    a, b = test_memory(path), test_memory(rpath)
    ok = len(a) >= 6 and a == b
    print(("ok   " if ok else "FAIL ") + f"test order: {len(a)} tests, identical memory records in both orders")
    if not ok:
        for k in sorted(set(a) | set(b)):
            if a.get(k) != b.get(k):
                print(f"     {k}: {a.get(k)} vs {b.get(k)}")
    return not ok


def check_repeat():
    files = []
    for d in (os.path.join(GC, "edge"), os.path.join(ROOT, "tests", "cases", "run")):
        files += [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith(".vo")
                  and not os.path.exists(os.path.join(d, n[:-3] + ".cmd"))]
    tmp = os.path.join(ROOT, "build", "tmp-gc-repeat")   # programs that write files write them here
    os.makedirs(tmp, exist_ok=True)

    def mem(p):
        return [records(subprocess.run([VO, "run", "--timeout", "60", "--mem-stats", "--json", p], cwd=tmp,
                                       capture_output=True, timeout=600).stderr) for _ in range(5)]
    bad = 0
    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for p, runs in zip(files, ex.map(mem, files)):
            if any(r != runs[0] for r in runs):
                print(f"FAIL repeat: {os.path.relpath(p, ROOT)} differs across 5 runs")
                bad += 1
    print(("ok   " if not bad else "FAIL ") + f"repeat: memory records identical across 5 runs for {len(files)} programs")
    return bad


def main():
    bad = check_workloads() + check_oom() + check_test_order()
    if "--repeat" in sys.argv:
        bad += check_repeat()
    print("gc_bounds: " + ("all checks passed" if not bad else f"{bad} failed"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
