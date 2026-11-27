"""The dependency graph: reachability, cycle groups, needed/blocked targets and priorities."""


def known_deps(b, name):
    return [d for d in b.targets[name][1] if d in b.targets]


def reach_sets(b):
    """name -> set of targets it reaches."""
    out = {}
    for name in b.order:
        seen, stack = set(), known_deps(b, name)
        while stack:
            d = stack.pop()
            if d not in seen:
                seen.add(d)
                stack.extend(known_deps(b, d))
        out[name] = seen
    return out


def cycle_groups(b, reach):
    cyclic = sorted(n for n in b.order if n in reach[n])
    groups, done = [], set()
    for n in cyclic:
        if n in done:
            continue
        g = sorted(m for m in cyclic if m == n or (m in reach[n] and n in reach[m]))
        done.update(g)
        groups.append(g)
    return groups


class Plan:
    def __init__(self, needed, blocked, buildable, priority, reach):
        self.needed = needed
        self.blocked = blocked
        self.buildable = buildable
        self.priority = priority
        self.reach = reach


def plan(b):
    reach = reach_sets(b)
    goals = b.goals if b.has_goal_line else list(b.order)
    needed = set()
    for g in goals:
        needed.add(g)
        needed |= reach[g]
    broken = {n for n in b.order if n in reach[n] or len(known_deps(b, n)) != len(b.targets[n][1])}
    blocked = {n for n in needed if n in broken or reach[n] & broken}
    buildable = needed - blocked
    priority = {}

    def prio(n):
        if n not in priority:
            ups = [prio(m) for m in buildable if n in b.targets[m][1]]
            priority[n] = b.targets[n][0] + max(ups, default=0)
        return priority[n]

    for n in buildable:
        prio(n)
    return Plan(needed, blocked, buildable, priority, reach), cycle_groups(b, reach)
