import os
import sys

LOWER = "abcdefghijklmnopqrstuvwxyz"
DIGITS = "0123456789"


class Fail(Exception):
    pass


def valid_name(n):
    return 1 <= len(n) <= 16 and n[0] in LOWER and all(c in LOWER or c in DIGITS or c == "-" for c in n)


def parse_profile(text):
    """(extends or None, {flag: definition})"""
    parent, flags, cur = None, {}, None
    for raw in text.split("\n"):
        line = raw.strip(" ")
        if line == "" or line.startswith("#"):
            continue
        w = line.split(" ")
        if w[0] == "extends":
            parent = w[1]
        elif w[0] == "flag":
            cur = {"default": w[3] == "on", "requires": [], "rules": [], "rollout": None}
            flags[w[1]] = cur
        elif w[0] == "requires":
            cur["requires"].append(w[1])
        elif w[0] == "when":
            cur["rules"].append((w[1], w[2], w[3], w[5] == "on"))
        elif w[0] == "rollout":
            cur["rollout"] = int(w[1])
    return parent, flags


def load(name):
    chain, layers = [], []
    while name is not None:
        if not valid_name(name):
            raise Fail(f"error: bad profile {name}")
        if name in chain:
            cyc = chain[chain.index(name):] + [name]
            raise Fail("error: extends cycle " + " -> ".join(cyc))
        path = f"flags/{name}.flags"
        if not os.path.isfile(path):
            raise Fail(f"error: no profile {name}")
        with open(path) as f:
            parent, flags = parse_profile(f.read())
        chain.append(name)
        layers.append(flags)
        name = parent
    merged = {}
    for flags in reversed(layers):
        merged.update(flags)
    return chain, merged


def self_dependent(flags):
    def reaches(start, target):
        seen, stack = set(), [start]
        while stack:
            f = stack.pop()
            for r in flags[f]["requires"]:
                if r == target:
                    return True
                if r in flags and r not in seen:
                    seen.add(r)
                    stack.append(r)
        return False
    return sorted(f for f in flags if reaches(f, f))


def version(s):
    parts = s.split(".")
    if not all(1 <= len(p) <= 9 and all(c in DIGITS for c in p) for p in parts):
        return None
    return [int(p) for p in parts]


def cmp_versions(a, b):
    n = max(len(a), len(b))
    a = a + [0] * (n - len(a))
    b = b + [0] * (n - len(b))
    return (a > b) - (a < b)


def holds(attrs, key, op, value):
    if key not in attrs:
        return False
    v = attrs[key]
    if op == "=":
        return v == value
    if op == "!=":
        return v != value
    if op == "in":
        return v in value.split(",")
    uv = version(v)
    if uv is None:
        return False
    c = cmp_versions(uv, version(value))
    return c >= 0 if op == ">=" else c < 0


def bucket(flag, uid):
    h = 0
    for b in (flag + ":" + uid).encode():
        h = (h * 31 + b) % 1000003
    return h % 100


def evaluate(flags, name, uid, attrs):
    d = flags[name]
    for r in d["requires"]:
        if r not in flags or not evaluate(flags, r, uid, attrs):
            return False
    for key, op, value, on in d["rules"]:
        if holds(attrs, key, op, value):
            return on
    if d["rollout"] is not None:
        return bucket(name, uid) < d["rollout"]
    return d["default"]


def main():
    lines = sys.stdin.read().split("\n")
    name = lines[0].split(" ", 1)[1]
    env = os.environ.get("FLAGS_PROFILE", "")
    if env != "":
        name = env
    try:
        chain, flags = load(name)
        cyc = self_dependent(flags)
        if cyc:
            raise Fail(f"error: requires cycle {cyc[0]}")
    except Fail as e:
        print(e)
        return
    print(f"profile {name}")
    print("chain " + " ".join(chain))
    counts = {f: 0 for f in flags}
    for line in lines[1:]:
        if line == "":
            continue
        w = line.split(" ")
        uid, attrs = w[1], {}
        for kv in w[2:]:
            k, v = kv.split("=", 1)
            attrs[k] = v
        on = sorted(f for f in flags if evaluate(flags, f, uid, attrs))
        for f in on:
            counts[f] += 1
        print(f"{uid} " + (",".join(on) if on else "-"))
    print("== counts")
    for f in sorted(counts):
        print(f"{f} {counts[f]}")


main()
