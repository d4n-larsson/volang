"""One function per command; each returns its output lines or raises RepoError."""
from repo import RepoError, changes, summary, byte_sorted, is_number
from merging import merge_base, three_way


def need_head(repo):
    if repo.head() is None:
        raise RepoError("no commits yet")
    return repo.head()


def write(repo, args):
    repo.work[args[0]] = " ".join(args[1:])
    return []


def delete(repo, args):
    if args[0] not in repo.work:
        raise RepoError(f"no such file {args[0]}")
    del repo.work[args[0]]
    return []


def record(repo, parents, message, tree):
    old = repo.head_snapshot()
    n = repo.make_commit(parents, message, tree)
    repo.branches[repo.current] = n
    repo.work = dict(tree)
    return [f"[{repo.current} {n}] {message} {summary(old, tree)}"]


def commit(repo, args):
    if not repo.dirty():
        raise RepoError("nothing to commit")
    h = repo.head()
    return record(repo, [] if h is None else [h], " ".join(args), repo.work)


def status(repo, args):
    return [f"on {repo.current}"] + (changes(repo.head_snapshot(), repo.work) or ["clean"])


def branch(repo, args):
    name = args[0]
    if is_number(name) or "~" in name:
        raise RepoError(f"invalid branch name {name}")
    if name in repo.branches:
        raise RepoError(f"branch {name} exists")
    target = repo.resolve(args[1]) if len(args) == 2 else need_head(repo)
    repo.branches[name] = target
    return []


def branches(repo, args):
    out = []
    for name in byte_sorted(repo.branches):
        c = repo.branches[name]
        mark = "*" if name == repo.current else " "
        out.append(f"{mark} {name} {'-' if c is None else c}")
    return out


def unbranch(repo, args):
    name = args[0]
    if name not in repo.branches:
        raise RepoError(f"no such branch {name}")
    if name == repo.current:
        raise RepoError("cannot delete the current branch")
    n = repo.branches[name]
    if not repo.contained(n, repo.head()):
        raise RepoError(f"branch {name} is not merged")
    del repo.branches[name]
    return [f"deleted branch {name} (was {n})"]


def checkout(repo, args):
    name = args[0]
    if name not in repo.branches:
        raise RepoError(f"no such branch {name}")
    if name == repo.current:
        raise RepoError(f"already on {name}")
    if repo.dirty():
        raise RepoError("uncommitted changes")
    repo.current = name
    repo.work = dict(repo.head_snapshot())
    return [f"switched to {name}"]


def reset(repo, args):
    n = repo.resolve(args[0])
    repo.branches[repo.current] = n
    repo.work = dict(repo.commits[n].snapshot)
    return [f"head is now {n}"]


def log(repo, args):
    n = repo.resolve(args[0]) if args else need_head(repo)
    out = []
    while True:
        c = repo.commits[n]
        line = f"{n} {c.message}"
        if len(c.parents) == 2:
            line += f" (merge of {c.parents[1]})"
        out.append(line)
        if not c.parents:
            return out
        n = c.parents[0]


def show(repo, args):
    c = repo.commits[repo.resolve(args[0])]
    out = [f"commit {c.number}",
           "parents " + (" ".join(map(str, c.parents)) if c.parents else "none"),
           f"message {c.message}"]
    for p in byte_sorted(c.snapshot):
        out.append(f'{p} "{c.snapshot[p]}"')
    return out


def diff(repo, args):
    a = repo.resolve(args[0])
    b = repo.resolve(args[1])
    return changes(repo.commits[a].snapshot, repo.commits[b].snapshot) or ["no differences"]


def merge(repo, args):
    theirs = repo.resolve(args[0])
    ours = need_head(repo)
    if repo.dirty():
        raise RepoError("uncommitted changes")
    if repo.contained(theirs, ours):
        return ["already up to date"]
    if repo.contained(ours, theirs):
        repo.branches[repo.current] = theirs
        repo.work = dict(repo.commits[theirs].snapshot)
        return [f"fast-forward to {theirs}"]
    b = merge_base(repo, ours, theirs)
    base = {} if b is None else repo.commits[b].snapshot
    merged, conflicts = three_way(base, repo.commits[ours].snapshot, repo.commits[theirs].snapshot)
    if conflicts:
        return [f"conflict: {p} ({k})" for p, k in conflicts] + ["merge aborted"]
    return record(repo, [ours, theirs], f"merge {args[0]} into {repo.current}", merged)


def apply_commit(repo, args, verb):
    """pick (verb "pick") and revert (verb "revert")."""
    n = repo.resolve(args[0])
    need_head(repo)
    if repo.dirty():
        raise RepoError("uncommitted changes")
    c = repo.commits[n]
    if len(c.parents) == 2:
        raise RepoError(f"cannot {verb} a merge")
    parent = repo.commits[c.parents[0]].snapshot if c.parents else {}
    head = repo.head_snapshot()
    if verb == "pick":
        merged, conflicts = three_way(parent, head, c.snapshot)
        message = f"{c.message} (picked from {n})"
    else:
        merged, conflicts = three_way(c.snapshot, head, parent)
        message = f"revert {n}"
    if conflicts:
        return [f"conflict: {p} ({k})" for p, k in conflicts] + [f"{verb} aborted"]
    if merged == head:
        raise RepoError(f"nothing to {verb}")
    return record(repo, [repo.head()], message, merged)


def pick(repo, args):
    return apply_commit(repo, args, "pick")


def revert(repo, args):
    return apply_commit(repo, args, "revert")


# name: (function, accepted argument counts as a predicate)
COMMANDS = {
    "write": (write, lambda n: n >= 1),
    "delete": (delete, lambda n: n == 1),
    "commit": (commit, lambda n: n >= 1),
    "status": (status, lambda n: n == 0),
    "branch": (branch, lambda n: n in (1, 2)),
    "branches": (branches, lambda n: n == 0),
    "unbranch": (unbranch, lambda n: n == 1),
    "pick": (pick, lambda n: n == 1),
    "revert": (revert, lambda n: n == 1),
    "checkout": (checkout, lambda n: n == 1),
    "reset": (reset, lambda n: n == 1),
    "log": (log, lambda n: n <= 1),
    "show": (show, lambda n: n == 1),
    "diff": (diff, lambda n: n == 2),
    "merge": (merge, lambda n: n == 1),
}
