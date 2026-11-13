def overlaps(a, b):
    return a["day"] == b["day"] and a["start"] < b["end"] and b["start"] < a["end"]


def conflicts(meetings):
    out = []
    for i, a in enumerate(meetings):
        for b in meetings[i + 1:]:
            if not overlaps(a, b):
                continue
            reasons = [f"room {a['room']}"] if a["room"] == b["room"] else []
            reasons += [f"person {p}" for p in sorted(set(a["people"]) & set(b["people"]))]
            if reasons:
                out.append(f"{a['id']} {b['id']}: " + ", ".join(reasons))
    return out


def room_minutes(meetings):
    out = []
    for room in sorted({m["room"] for m in meetings}):
        total = 0
        for day in {m["day"] for m in meetings}:
            spans = sorted((m["start"], m["end"]) for m in meetings if m["room"] == room and m["day"] == day)
            cur_s = cur_e = None
            for s, e in spans:
                if cur_e is None or s > cur_e:
                    if cur_e is not None:
                        total += cur_e - cur_s
                    cur_s, cur_e = s, e
                else:
                    cur_e = max(cur_e, e)
            if cur_e is not None:
                total += cur_e - cur_s
        out.append(f"{room} {total}")
    return out
