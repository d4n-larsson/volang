from gradebook import rdiv
from parsing import pct


def letter(f):
    for bound, l in ((9000, "A"), (8000, "B"), (7000, "C"), (6000, "D")):
        if f >= bound:
            return l
    return "F"


def report(gb):
    finals = {s: gb.final(s) for s in gb.students}
    order = sorted(gb.students, key=lambda s: (-finals[s], s))
    out = ["students:"] + [f"{s} {pct(finals[s])} {letter(finals[s])}" for s in order]
    c = len(order)
    out += ["stats:", f"count {c}"]
    if c:
        fs = sorted(finals.values())
        med = fs[c // 2] if c % 2 else rdiv(fs[c // 2 - 1] + fs[c // 2], 2)
        top = fs[-1]
        out += [f"mean {pct(rdiv(sum(fs), c))}", f"median {pct(med)}",
                f"top {pct(top)} " + ",".join(sorted(s for s in order if finals[s] == top)),
                "letters " + " ".join(f"{l}:{sum(letter(f) == l for f in fs)}" for l in "ABCDF")]
    out.append("assignments:")
    for a in gb.assignments:
        total = sum(gb.bp(s, a) for s in gb.students)
        missing = sum((s, a) not in gb.scores for s in gb.students)
        out.append(f"{a} avg {pct(rdiv(total, c) if c else 0)} missing {missing}")
    return out
