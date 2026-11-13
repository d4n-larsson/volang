def earliest(meetings, day, minutes, people):
    busy = [m for m in meetings if m["day"] == day and set(m["people"]) & set(people)]
    for t in sorted({480} | {m["end"] for m in busy}):
        if t >= 480 and t + minutes <= 1080 and all(not (m["start"] < t + minutes and t < m["end"]) for m in busy):
            return t
    return None
