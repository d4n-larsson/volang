import os
import sys

NAME_CHARS = set("abcdefghijklmnopqrstuvwxyz0123456789-")
KEYWORDS = ("source", "target", "exclude-name", "exclude-suffix", "exclude-dir")


def normalize(d):
    """The normalized directory, or None when it starts with / or climbs out."""
    if d.startswith("/"):
        return None
    kept = []
    for part in d.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            if not kept:
                return None
            kept.pop()
        else:
            kept.append(part)
    return "/".join(kept)


def allowed(d, root):
    n = normalize(d)
    if n is not None and (n == root or n.startswith(root + "/")):
        return n
    return None


def parse_job(text):
    """(config, None) or (None, problem)."""
    conf = {"excludes": []}
    for n, line in enumerate(text.split("\n"), start=1):
        if line == "" or line.startswith("#"):
            continue
        key, sep, value = line.partition(" ")
        if not sep or key not in KEYWORDS or value == "" or " " in value:
            return None, f"bad config line {n}"
        if key in ("source", "target"):
            if key in conf:
                return None, f"duplicate {key}"
            conf[key] = value
        else:
            conf["excludes"].append((key, value))
    for key in ("source", "target"):
        if key not in conf:
            return None, f"missing {key}"
    return conf, None


def excluded(rel, excludes):
    base = rel.rsplit("/", 1)[-1]
    for key, value in excludes:
        if key == "exclude-name" and base == value:
            return True
        if key == "exclude-suffix" and base.endswith(value):
            return True
        if key == "exclude-dir" and rel.startswith(value + "/"):
            return True
    return False


def files_below(d):
    out = []
    for dirpath, _, names in os.walk(d):
        for n in names:
            out.append(os.path.relpath(os.path.join(dirpath, n), d))
    return sorted(out)


def read(path):
    with open(path) as f:
        return f.read()


def do_job(verb, job):
    if not (1 <= len(job) <= 20 and all(c in NAME_CHARS for c in job)):
        return print("bad job name")
    path = f"sync/{job}.conf"
    if not os.path.isfile(path):
        return print("no such job")
    conf, problem = parse_job(read(path))
    if problem:
        return print(problem)
    src = allowed(conf["source"], "src")
    if src is None:
        return print("source not allowed: " + conf["source"])
    dst = allowed(conf["target"], "mirror")
    if dst is None:
        return print("target not allowed: " + conf["target"])
    if not os.path.isdir(src):
        return print("source missing")
    ex = conf["excludes"]
    sources = [r for r in files_below(src) if not excluded(r, ex)]
    n_excluded = len(files_below(src)) - len(sources)
    targets = [r for r in files_below(dst) if not excluded(r, ex)] if os.path.isdir(dst) else []
    counts = {"copy": 0, "update": 0, "unchanged": 0, "conflict": 0}
    writes = []
    for rel in sources:
        parts = rel.split("/")
        t = dst + "/" + rel
        blocked = os.path.isdir(t) or any(
            os.path.isfile(dst + "/" + "/".join(parts[:j])) for j in range(1, len(parts)))
        if blocked:
            action = "conflict"
        elif not os.path.exists(t):
            action = "copy"
        elif read(t) != read(src + "/" + rel):
            action = "update"
        else:
            action = "unchanged"
        counts[action] += 1
        if action != "unchanged":
            print(f"{action} {rel}")
        if action in ("copy", "update"):
            writes.append(rel)
    source_set = set(sources)
    stale = [r for r in targets if r not in source_set]
    for r in stale:
        print("stale " + r)
    print(f"copy {counts['copy']}, update {counts['update']}, unchanged {counts['unchanged']}, "
          f"conflict {counts['conflict']}, stale {len(stale)}, excluded {n_excluded}")
    if verb == "run":
        # remember when each job last ran, for the next report
        with open(".sync-state", "a") as state:
            state.write(job + "\n")
        for rel in writes:
            t = dst + "/" + rel
            os.makedirs(os.path.dirname(t), exist_ok=True)
            with open(t, "w") as f:
                f.write(read(src + "/" + rel))


def main():
    for k, line in enumerate(sys.stdin.read().split("\n"), start=1):
        if line == "":
            continue
        f = line.split(" ")
        if len(f) != 2 or f[0] not in ("plan", "run"):
            print(f"error: line {k}: bad command")
            continue
        print("== " + line)
        do_job(f[0], f[1])


main()
