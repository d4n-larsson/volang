from logline import LEVELS, entries


def stats(lines):
    valid = len(entries(lines))
    return ["lines " + str(len(lines)) + " valid " + str(valid) + " invalid " + str(len(lines) - valid)]


def levels(lines):
    counts = {lv: 0 for lv in LEVELS}
    for e in entries(lines):
        counts[e.level] += 1
    return [lv + " " + str(counts[lv]) for lv in LEVELS]


def errors(lines):
    per = {}
    for e in entries(lines):
        if e.level == "ERROR":
            per[e.component] = per.get(e.component, 0) + 1
    if not per:
        return ["no errors"]
    names = sorted(per)
    names.sort(key=lambda c: -per[c])
    return [c + " " + str(per[c]) for c in names]
