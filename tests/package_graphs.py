#!/usr/bin/env python3
"""Spec 061 (SC-002, research R8): version selection over generated dependency graphs.

  package_graphs.py [--count N] [--seed S]   N graphs (default 50)
  package_graphs.py --hundred DIR            writes a program with 100 local packages (SC-005) and exits

Each graph is a set of packages with several versions, each version requiring versions of later
packages. Its roots are added to a fresh project in every order (at most 24 orders). Every order must
give the manifest a selection computed here gives for that order, and a program that checks. In every
other graph a higher version requires at least what the one below it does: there every order must give
a byte-identical vo.packages.

Then graphs the selection must refuse or get through (US2): a conflict (several packages requiring one
package at 1.x, one at 2.0.0: E0707 naming every requirer with its version), a source clash (E0708 naming
both sources), each leaving the manifest as it was, and a cycle of requirements, which resolves.
Last, restore (SC-003): a 10-package program with its package directories deleted is restored and checks;
then, for each package in turn, its pin is changed (as if the version's content had been changed after it
was pinned) and its directory deleted: restore must refuse it (E0701) and leave no directory.
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


def pkg(repos, name, version, deps):
    """A package with one function; deps: [(name, version, repository)] of its manifest."""
    d = os.path.join(repos, name, version)
    os.makedirs(d)
    open(os.path.join(d, name + "m.vo"), "w").write("export fn v() -> int {\n    return 1;\n}\n")
    if deps:
        open(os.path.join(d, "vo.packages"), "w").write("".join(
            f"{x} packages/{x} {{hash:{src}@{dv}}} https://127.0.0.1:{{port}}/{src}@{dv}\n" for x, dv, src in deps))


def special(r, repos, count):
    """The US2 graphs: [(label, [(name, version)] to add in order, None or (code, texts the failing last
    add must print))]."""
    out = []
    for g in range(count):
        kind = ("conflict", "clash", "cycle")[g % 3]
        p = f"s{g}"
        if kind == "conflict":
            minors = [r.randint(0, 3) for _ in range(r.randint(1, 4))]
            for m in sorted(set(minors)):
                pkg(repos, p + "x", f"1.{m}.0", [])
            pkg(repos, p + "x", "2.0.0", [])
            for i, m in enumerate(minors):
                pkg(repos, f"{p}r{i}", "1.0.0", [(p + "x", f"1.{m}.0", p + "x")])
            pkg(repos, p + "top", "1.0.0", [(p + "x", "2.0.0", p + "x")])
            adds = [(f"{p}r{i}", "1.0.0") for i in range(len(minors))]
            r.shuffle(adds)
            texts = [f"1.{m}.0 by `{p}r{i}`" for i, m in enumerate(minors)] + [f"2.0.0 by `{p}top`"]
            out.append((f"{kind} {g}", adds + [(p + "top", "1.0.0")], ("E0707", texts)))
        elif kind == "clash":
            pkg(repos, p + "x", "1.0.0", [])
            pkg(repos, p + "y", "1.0.0", [])
            pkg(repos, p + "a", "1.0.0", [(p + "x", "1.0.0", p + "x")])
            pkg(repos, p + "b", "1.0.0", [(p + "x", "1.0.0", p + "y")])
            out.append((f"{kind} {g}", [(p + "a", "1.0.0"), (p + "b", "1.0.0")], ("E0708", [f"/{p}x (", f"/{p}y (`{p}b`)"])))
        else:
            n = r.randint(2, 5)
            for i in range(n):
                pkg(repos, f"{p}c{i}", "1.0.0", [(f"{p}c{(i + 1) % n}", "1.0.0", f"{p}c{(i + 1) % n}")])
            out.append((f"{kind} {g} of {n}", [(f"{p}c0", "1.0.0")], None))
    return out


def run_special(work, port, label, adds, refusal):
    """A problem, or None."""
    d = os.path.join(work, label.replace(" ", "-"))
    os.makedirs(d)
    open(os.path.join(d, "main.vo"), "w").write("fn main() uses console {\n    print(\"x\");\n}\n")
    manifest = os.path.join(d, "vo.packages")
    for k, (n, v) in enumerate(adds):
        before = open(manifest).read() if os.path.exists(manifest) else None
        r = subprocess.run([VO, "package", "--ca-file", CA, "main.vo", n, f"https://127.0.0.1:{port}/{n}@{v}"],
                           cwd=d, capture_output=True, text=True)
        if refusal and k == len(adds) - 1:
            code, texts = refusal
            missing = [t for t in texts if t not in r.stdout]
            if r.returncode != 65 or f"error[{code}]" not in r.stdout or missing:
                return f"{label}: wanted {code} with {missing or texts}, got status {r.returncode} {r.stdout[-400:]}"
            if (open(manifest).read() if os.path.exists(manifest) else None) != before:
                return f"{label}: the refused add changed vo.packages"
            if [x for x in os.listdir(os.path.join(d, "packages")) if ".new-" in x or ".old-" in x]:
                return f"{label}: the refused add left staging folders"
        elif r.returncode:
            return f"{label}: add {n}@{v}: status {r.returncode} {r.stdout[-300:]}{r.stderr[-200:]}"
    if not refusal:
        want = sorted(n for n in os.listdir(os.path.join(d, "packages")))
        got = sorted(line.split()[0] for line in open(manifest).read().splitlines()[1:])
        ring = int(label.split()[-1])
        if got != want or len(got) != ring:
            return f"{label}: manifest has {got}, packages/ has {want}"
    c = subprocess.run([VO, "check", "main.vo"], cwd=d, capture_output=True, text=True)
    return f"{label}: check: {c.stdout[-400:]}" if c.returncode else None


def restore_check(work, port, repos_made):
    """(problems, trials refused). The repositories t0 (requiring t1..t9) were written by special()."""
    d = os.path.join(work, "ten")
    os.makedirs(d)
    open(os.path.join(d, "main.vo"), "w").write("import t0.t0m;\n\nfn main() uses console {\n    print(str(t0m.v()));\n}\n")

    def vo(*words):
        return subprocess.run([VO, *words], cwd=d, capture_output=True, text=True)
    r = vo("package", "--ca-file", CA, "main.vo", "t0", f"https://127.0.0.1:{port}/t0")
    manifest = os.path.join(d, "vo.packages")
    text = open(manifest).read() if r.returncode == 0 else ""
    lines = text.splitlines()[1:]
    if len(lines) != 10:
        return [f"restore: the add gave {len(lines)} packages: {r.stdout[-300:]}"], 0
    shutil.rmtree(os.path.join(d, "packages"))
    r = vo("package", "--ca-file", CA, "--restore", "main.vo")
    c = vo("check", "main.vo")
    if r.returncode or r.stdout.count("restored ") != 10 or c.returncode or open(manifest).read() != text:
        return [f"restore: status {r.returncode}, check {c.returncode}: {r.stdout[-300:]}{c.stdout[-300:]}"], 0
    problems, refused = [], 0
    for line in lines:
        name, h = line.split()[0], line.split()[2]
        moved = h[:-1] + ("0" if h[-1] != "0" else "1")
        open(manifest, "w").write(text.replace(h, moved))
        shutil.rmtree(os.path.join(d, "packages", name))
        r = vo("package", "--ca-file", CA, "--restore", "main.vo")
        if r.returncode == 65 and f"error[E0701]: Package `{name}`" in r.stdout and not os.path.exists(os.path.join(d, "packages", name)):
            refused += 1
        else:
            problems.append(f"restore: a changed `{name}` was not refused: status {r.returncode} {r.stdout[-300:]}")
        open(manifest, "w").write(text)
        vo("package", "--ca-file", CA, "--restore", "main.vo")
    return problems, refused


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
    problems, orders_run, refused = [], 0, 0
    try:
        graphs = [gen(r, g, g % 2 == 0) for g in range(count)]  # every other graph only grows
        repos = os.path.join(work, "repos")
        for pk, _ in graphs:
            write_repos(repos, pk)
        specials = special(r, repos, max(3, count // 4))
        for i in range(1, 10):
            pkg(repos, f"t{i}", "1.0.0", [])
        pkg(repos, "t0", "1.0.0", [(f"t{i}", "1.0.0", f"t{i}") for i in range(1, 10)])
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
            for label, adds, refusal in specials:
                p = run_special(work, port, label, adds, refusal)
                if p:
                    problems.append(p)
            more, refused = restore_check(work, port, repos)
            problems += more
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for p in problems[:15]:
        print("FAIL", p)
    print(f"package_graphs: {count} graphs, {orders_run} orders, {len(specials)} conflicts, clashes and cycles, "
          f"10 packages restored and {refused} of 10 changed versions refused, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
