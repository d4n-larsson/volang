import sys

import graph
import lines
import sim


def names(xs):
    return " ".join(xs) if xs else "-"


def main():
    b = lines.read(sys.stdin.read())
    out = [f"line {i}: {text}" for i, text in b.problems]
    p, groups = graph.plan(b)
    out += ["cycle: " + " ".join(g) for g in groups]
    r = sim.simulate(b, p)
    out += r.log
    workers = b.settings["workers"]
    out.append(f"makespan {r.makespan}")
    total = sum(r.busy)
    out.append(f"utilization {total * 100 // (workers * r.makespan) if r.makespan else 0}%")
    for w in range(workers):
        out.append(f"w{w + 1} busy {r.busy[w]} attempts {r.attempts[w]}")
    out.append("built: " + names(r.built))
    out.append("failed: " + names(sorted(r.failed)))
    out.append("skipped: " + names(sorted(r.skipped)))
    out.append("blocked: " + names(sorted(p.blocked)))
    sys.stdout.write("".join(s + "\n" for s in out))


main()
