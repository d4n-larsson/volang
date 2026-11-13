from logline import entries, to_seconds


def busiest(lines):
    per_hour = [0] * 24
    found = entries(lines)
    if not found:
        return ["no entries"]
    for e in found:
        per_hour[e.hour] += 1
    best = 0
    for h in range(24):
        if per_hour[h] > per_hour[best]:
            best = h
    label = str(best)
    if len(label) < 2:
        label = "0" + label
    return ["busiest hour " + label + " with " + str(per_hour[best]) + " entries"]


def in_range(lines, args):
    lo = to_seconds(args[0], args[1])
    hi = to_seconds(args[2], args[3])
    if lo < 0 or hi < 0:
        return ["error: bad time"]
    out = [e.text for e in entries(lines) if lo <= e.seconds <= hi]
    return out + [str(len(out)) + " entries in range"]


def span(lines):
    found = entries(lines)
    if not found:
        return ["no entries"]
    first = min(e.seconds for e in found)
    last = max(e.seconds for e in found)
    return ["span " + str(last - first) + " seconds"]
