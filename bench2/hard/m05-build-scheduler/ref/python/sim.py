"""The build simulation: workers, the ready pool, failures, retries and skips."""


class Result:
    def __init__(self, workers):
        self.log = []
        self.busy = [0] * workers
        self.attempts = [0] * workers
        self.built = []
        self.failed = []
        self.skipped = set()
        self.makespan = 0


def may_start(b, running, t):
    if t not in b.pool_of:
        return True
    pool = b.pool_of[t]
    busy = sum(1 for x in running if x is not None and b.pool_of.get(x[1]) == pool)
    return busy < b.pools[pool]


def simulate(b, p):
    workers = b.settings["workers"]
    retries, backoff, timeout = b.settings["retries"], b.settings["backoff"], b.settings["timeout"]
    r = Result(workers)
    running = [None] * workers          # (end, target, attempt, outcome) per worker
    tries = {t: 0 for t in p.buildable}
    succeeded = set()
    waiting = {}                         # target -> time it rejoins the pool
    pool = []
    dependents = {t: [m for m in p.buildable if t in b.targets[m][1]] for t in p.buildable}
    for t in p.buildable:
        if not b.targets[t][1]:
            waiting[t] = 0
    now = 0
    while True:
        times = [x[0] for x in running if x is not None] + list(waiting.values())
        if not times:
            break
        now = min(times)
        # step 1: attempts ending now
        for w in range(workers):
            if running[w] is None or running[w][0] != now:
                continue
            _, t, a, outcome = running[w]
            running[w] = None
            if outcome == "done":
                r.log.append(f"{now} done {t} w{w + 1}")
                r.built.append(t)
                succeeded.add(t)
                for m in dependents[t]:
                    if m not in r.skipped and all(d in succeeded for d in b.targets[m][1]):
                        waiting[m] = now
                continue
            r.log.append(f"{now} {outcome} {t} w{w + 1} #{a}")
            if tries[t] < retries + 1:
                waiting[t] = now + backoff
                continue
            r.log.append(f"{now} give up {t}")
            r.failed.append(t)
            for m in sorted(m for m in p.buildable if t in p.reach[m] and m not in r.skipped):
                r.log.append(f"{now} skip {m}")
                r.skipped.add(m)
        # step 2: targets joining the pool now
        for t in [t for t, at in waiting.items() if at == now]:
            del waiting[t]
            pool.append(t)
        # step 3: free workers take the best ready targets
        for w in range(workers):
            if running[w] is not None:
                continue
            startable = sorted((t for t in pool if may_start(b, running, t)), key=lambda t: (-p.priority[t], t))
            if not startable:
                break
            t = startable[0]
            pool.remove(t)
            tries[t] += 1
            d = b.targets[t][0]
            outcome = "done"
            if timeout and d > timeout:
                d, outcome = timeout, "timeout"
            elif (t, tries[t]) in b.fails:
                outcome = "fail"
            running[w] = (now + d, t, tries[t], outcome)
            r.busy[w] += d
            r.attempts[w] += 1
            r.makespan = max(r.makespan, now + d)
            r.log.append(f"{now} start {t} w{w + 1} #{tries[t]}")
    return r
