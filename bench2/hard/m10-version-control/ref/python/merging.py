"""Three-way merge of snapshots."""
from repo import byte_sorted


def merge_base(repo, ours, theirs):
    common = repo.ancestry(ours) & repo.ancestry(theirs)
    return max(common) if common else None


def three_way(base, ours, theirs):
    """Returns (merged tree, [(path, kind)] conflicts)."""
    merged, conflicts = {}, []
    for p in byte_sorted(set(base) | set(ours) | set(theirs)):
        b, o, t = base.get(p), ours.get(p), theirs.get(p)
        if o == t:
            r = o
        elif o == b:
            r = t
        elif t == b:
            r = o
        else:
            if b is None:
                kind = "both added"
            elif o is None:
                kind = "deleted by us"
            elif t is None:
                kind = "deleted by them"
            else:
                kind = "both modified"
            conflicts.append((p, kind))
            continue
        if r is not None:
            merged[p] = r
    return merged, conflicts
