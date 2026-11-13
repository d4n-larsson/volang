def line(t, mark_done=False):
    s = f"#{t.id} P{t.prio} {t.text}" + "".join(" +" + g for g in sorted(t.tags))
    return s + " (done)" if mark_done and t.done else s


def listing(store, which):
    ts = store.all()
    if which is None:
        ts = sorted([t for t in ts if not t.done], key=lambda t: (t.prio, t.id))
        out = [line(t) for t in ts]
    elif which == "done":
        ts = sorted([t for t in ts if t.done], key=lambda t: t.done_seq)
        out = [line(t) for t in ts]
    else:
        out = [line(t, True) for t in ts if which[1:] in t.tags]
    return out or ["no tasks"]


def stats(store):
    ts = store.all()
    done = sum(t.done for t in ts)
    return f"open {len(ts) - done} done {done}"
