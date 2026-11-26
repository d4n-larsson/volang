#!/usr/bin/env python3
"""Spec 061 (SC-002, research R8): version selection over generated dependency graphs.

  package_graphs.py [--count N] [--seed S]   N graphs (default 50)
  package_graphs.py --hundred DIR            writes a program with 100 local packages (SC-005) and exits

Each graph is a set of packages with several versions, each version requiring versions of later
packages. Its roots are added to a fresh project in every order (at most 24 orders). Every order must
give the manifest a selection computed here gives for that order, and a program that checks. In every
other graph a higher version requires at least what the one below it does: there every order must give
a byte-identical vo.packages.
"""
import itertools
import os
import random
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
CA = os.path.join(ROOT, "tests", "cases", "packages", "fetch", "test-ca.pem")
sys.path.insert(0, os.path.join(ROOT, "tests"))
import git_server  # noqa: E402


def gen(r, g, growing):
    """A graph: {name: {version: {dependency: version}}}, and its roots [(name, version)]. When growing,
    a higher version requires everything the one below it does, at the same or a higher version."""
    names = [f"g{g}p{i}" for i in range(r.randint(3, 12))]
    pk = {}
    for i in reversed(range(len(names))):
        versions = [f"1.{m}.0" for m in range(r.randint(1, 4))]
        pk[names[i]] = {}
        later = names[i + 1:i + 6]
        below = {}
        for v in versions:
            deps = r.sample(later, min(len(later), r.choice([0, 1, 1, 2, 3])))
            now = {d: r.choice(sorted(pk[d])) for d in deps}
            if growing:
                for d, dv in below.items():
                    higher = [x for x in sorted(pk[d]) if key(x) >= key(dv)]
                    now[d] = r.choice(higher) if d not in now or key(now[d]) < key(dv) else now[d]
            pk[names[i]][v] = below = now
    pool = names[:max(2, len(names) // 2)]
    roots = [(n, r.choice(sorted(pk[n]))) for n in r.sample(pool, min(len(pool), r.choice([2, 2, 3, 3, 4])))]
    return pk, roots


def write_repos(repos, pk):
    for name, versions in pk.items():
        for v, deps in versions.items():
            d = os.path.join(repos, name, v)
            os.makedirs(d)
            imports = "".join(f"import {x}.{x}m;\n" for x in sorted(deps))
            calls = "".join(f" + {x}m.v()" for x in sorted(deps))
            open(os.path.join(d, name + "m.vo"), "w").write(
                f"{imports}{chr(10) if imports else ''}export fn v() -> int {{\n    return {int(v.split('.')[1])}{calls};\n}}\n")
            if deps:
                open(os.path.join(d, "vo.packages"), "w").write("".join(
                    f"{x} packages/{x} {{hash:{x}@{dv}}} https://127.0.0.1:{{port}}/{x}@{dv}\n" for x, dv in sorted(deps.items())))


def key(v):
    return [int(x) for x in v.split(".")]


def select(pk, order):
    """What the adds of `order` leave (research R4): name -> (version, indirect). Each add starts from the
    manifest's versions with the named package at the version asked, reads in rounds every package not
    yet read at its version, raises what they require, and drops what the program's own packages no
    longer reach."""
    state = {}
    for name, version in order:
        sel = {n: v for n, (v, _) in state.items()}
        direct = {n for n, (_, ind) in state.items() if not ind} | {name}
        sel[name] = version
        read, needs = {}, {}
        while True:
            todo = [n for n in sel if read.get(n) != sel[n]]
            if not todo:
                break
            pending = []
            for n in todo:
                read[n] = sel[n]
                needs[n] = list(pk[n][sel[n]])
                pending += pk[n][sel[n]].items()
            for d, dv in pending:
                if d not in sel or key(sel[d]) < key(dv):
                    sel[d] = dv
        reach, todo = set(), list(direct)
        while todo:
            n = todo.pop()
            if n not in reach:
                reach.add(n)
                todo += needs[n]
        state = {n: (sel[n], n not in direct) for n in reach}
    return state


def run_order(work, port, order, k):
    d = os.path.join(work, f"o{k}")
    os.makedirs(d)
    open(os.path.join(d, "main.vo"), "w").write(
        "".join(f"import {n}.{n}m;\n" for n, _ in sorted(order)) + "\nfn main() uses console {\n    print(str("
        + " + ".join(f"{n}m.v()" for n, _ in sorted(order)) + "));\n}\n")
    for n, v in order:
        r = subprocess.run([VO, "package", "--ca-file", CA, "main.vo", n, f"https://127.0.0.1:{port}/{n}@{v}"],
                           cwd=d, capture_output=True, text=True)
        if r.returncode:
            return None, f"add {n}@{v}: status {r.returncode} {r.stdout[-300:]}{r.stderr[-200:]}"
    c = subprocess.run([VO, "check", "main.vo"], cwd=d, capture_output=True, text=True)
    if c.returncode:
        return None, f"check: {c.stdout[-400:]}"
    return open(os.path.join(d, "vo.packages")).read(), None


def hundred(d):
    os.makedirs(d, exist_ok=True)
    for i in range(100):
        os.makedirs(os.path.join(d, "lib", f"p{i}"), exist_ok=True)
        open(os.path.join(d, "lib", f"p{i}", f"p{i}m.vo"), "w").write(f"export fn v() -> int {{\n    return {i};\n}}\n")
    open(os.path.join(d, "main.vo"), "w").write(
        "".join(f"import p{i}.p{i}m;\n" for i in range(100)) + "\nfn main() uses console {\n    print(str("
        + " + ".join(f"p{i}m.v()" for i in range(100)) + "));\n}\n")
    for i in range(100):
        subprocess.run([VO, "package", "main.vo", f"p{i}", f"lib/p{i}"], cwd=d, capture_output=True, check=True)


def main():
    args = sys.argv[1:]
    if "--hundred" in args:
        hundred(args[args.index("--hundred") + 1])
        return 0
    count = int(args[args.index("--count") + 1]) if "--count" in args else 50
    r = random.Random(int(args[args.index("--seed") + 1]) if "--seed" in args else 1)
    work = tempfile.mkdtemp(prefix="vo-graphs-")
    problems, orders_run = [], 0
    try:
        graphs = [gen(r, g, g % 2 == 0) for g in range(count)]  # every other graph only grows
        repos = os.path.join(work, "repos")
        for pk, _ in graphs:
            write_repos(repos, pk)
        port, tmp = git_server.start("a.test", repos, [])
        try:
            jobs = []
            with ThreadPoolExecutor(8) as pool:
                for g, (pk, roots) in enumerate(graphs):
                    orders = list(itertools.permutations(roots))[:24]
                    gw = os.path.join(work, f"g{g}")
                    jobs.append((g, pk, roots, [pool.submit(run_order, gw, port, o, k) for k, o in enumerate(orders)], orders))
                for g, pk, roots, futures, orders in jobs:
                    results = [f.result() for f in futures]
                    orders_run += len(results)
                    bad = [e for _, e in results if e]
                    if bad:
                        problems.append(f"graph {g} roots {roots}: {bad[0]}")
                        continue
                    if g % 2 == 0 and len({t for t, _ in results}) != 1:  # a growing graph: one result for every order
                        problems.append(f"graph {g} roots {roots}: {len({t for t, _ in results})} different manifests over {len(results)} orders")
                        continue
                    for order, (text, _) in zip(orders, results):
                        got = {}
                        for line in text.splitlines()[1:]:
                            f = line.split()
                            got[f[0]] = (f[3].rsplit("@", 1)[1], len(f) > 4)
                        want = select(pk, order)
                        if got != want:
                            diff = {n: (got.get(n), want.get(n)) for n in set(got) | set(want) if got.get(n) != want.get(n)}
                            problems.append(f"graph {g} order {order}: (got, want) {diff}")
                            break
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for p in problems[:15]:
        print("FAIL", p)
    print(f"package_graphs: {count} graphs, {orders_run} orders, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
