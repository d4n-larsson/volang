# The dependency graph: order, successors and cycles.


def ordered(plan):
    """All tasks in creation order."""
    return sorted(plan.tasks.values(), key=lambda t: t.seq)


def depends_on(task, pred):
    return any(d.pred == pred for d in task.preds)


def successors(plan, tid):
    """IDs of the tasks that depend on tid, in byte order."""
    out = [t.tid for t in plan.tasks.values() if depends_on(t, tid)]
    out.sort()
    return out


def path_to(plan, current, target, seen):
    """A chain current -> ... -> target in which each task depends on the next, or None.
    Predecessors are tried in the order they were added."""
    if current == target:
        return [current]
    seen.add(current)
    for d in plan.tasks[current].preds:
        if d.pred in seen:
            continue
        rest = path_to(plan, d.pred, target, seen)
        if rest is not None:
            return [current] + rest
    return None


def cycle_with(plan, tid, pred):
    """The cycle `tid depends on pred` would close, as a list of IDs, or None."""
    rest = path_to(plan, pred, tid, set())
    if rest is None:
        return None
    return [tid] + rest


def topo_order(plan):
    """Every task after all its predecessors; among the ready ones the earliest created first."""
    placed = set()
    out = []
    pending = ordered(plan)
    while len(pending) > 0:
        for i, t in enumerate(pending):
            if all(d.pred in placed for d in t.preds):
                out.append(t)
                placed.add(t.tid)
                pending.pop(i)
                break
    return out
