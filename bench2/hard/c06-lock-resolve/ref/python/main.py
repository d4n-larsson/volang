import os
import sys
from collections import deque

LOWER = "abcdefghijklmnopqrstuvwxyz"
DIGITS = "0123456789"


class Stop(Exception):
    pass


def valid_lock(n):
    return 1 <= len(n) <= 32 and all(c in LOWER or c in DIGITS or c in "-_" for c in n)


def valid_package(n):
    return 1 <= len(n) <= 30 and n[0] in LOWER and all(c in LOWER or c in DIGITS or c == "-" for c in n)


def ver(s):
    return tuple(int(p) for p in s.split("."))


def satisfies(v, constraint):
    for term in constraint.split(","):
        if term == "*":
            continue
        for op in (">=", "=", "<", "^", "~"):
            if term.startswith(op):
                w = ver(term[len(op):])
                break
        if op == "=" and v != w:
            return False
        if op == ">=" and v < w:
            return False
        if op == "<" and v >= w:
            return False
        if op == "^" and (v[0] != w[0] or v < w):
            return False
        if op == "~" and (v[:2] != w[:2] or v < w):
            return False
    return True


def parse_registry(text):
    """[(version text, [(dep, constraint)])]"""
    versions = []
    for raw in text.split("\n"):
        line = raw.strip(" ")
        if line == "" or line.startswith("#"):
            continue
        w = line.split(" ")
        if w[0] == "version":
            versions.append((w[1], []))
        elif w[0] == "dep":
            versions[-1][1].append((w[1], w[2]))
    return versions


def resolve(roots):
    constraints, queue, picked, order = {}, deque(), {}, []

    def add(pkg, c, requirer):
        if pkg not in constraints:
            constraints[pkg] = []
            queue.append(pkg)
        constraints[pkg].append((c, requirer))

    for pkg, c in roots:
        add(pkg, c, "root")
    while queue:
        p = queue.popleft()
        requirer = constraints[p][0][1]
        if not valid_package(p):
            raise Stop(f"error: bad package name {p} (required by {requirer})")
        path = f"registry/{p}.txt"
        if not os.path.isfile(path):
            raise Stop(f"error: unknown package {p} (required by {requirer})")
        with open(path) as f:
            versions = parse_registry(f.read())
        ok = [(ver(v), v, deps) for v, deps in versions
              if all(satisfies(ver(v), c) for c, _ in constraints[p])]
        if not ok:
            listed = ", ".join(f"{c} ({r})" for c, r in constraints[p])
            raise Stop(f"error: no version of {p} satisfies {listed}")
        _, v, deps = max(ok)
        picked[p] = v
        print(f"pick {p} {v}")
        for d, c in deps:
            add(d, c, f"{p}@{v}")
            if d in picked and not satisfies(ver(picked[d]), c):
                raise Stop(f"error: conflict on {d}: {d}@{picked[d]} does not satisfy {c} (required by {p}@{v})")
    return picked


def main():
    lines = sys.stdin.read().split("\n")
    name = lines[0].split(" ", 1)[1]
    if not valid_lock(name):
        print(f"error: bad lock name {name}")
        return
    roots = [tuple(l.split(" ")) for l in lines[1:] if l != ""]
    try:
        picked = resolve(roots)
    except Stop as e:
        print(e)
        return
    lock = [f"{p} {picked[p]}" for p in sorted(picked)]
    print("== lock")
    for l in lock:
        print(l)
    with open(f"out/{name}.lock", "w") as f:
        f.write("".join(l + "\n" for l in lock))
    print(f"wrote out/{name}.lock {len(lock)}")


main()
