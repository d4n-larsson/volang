import sys

import render
from commands import parse
from store import Store


def handle(store, line):
    p = parse(line)
    if p is None:
        return ["error: bad command"]
    c, f = p
    if c == "":
        return []
    if c == "add":
        return [f"added #{store.add(int(f[0]), f[1])}"]
    if c == "list":
        return render.listing(store, f[0] if f else None)
    if c == "stats":
        return [render.stats(store)]
    t = store.get(f[0])
    if t is None:
        return [f"error: no task {f[0]}"]
    if c == "done":
        return [f"done #{t.id}" if store.set_done(t, True) else f"#{t.id} is already done"]
    if c == "reopen":
        return [f"reopened #{t.id}" if store.set_done(t, False) else f"#{t.id} is already open"]
    if c == "delete":
        store.delete(t)
        return [f"deleted #{t.id}"]
    if c == "tag":
        return [f"tagged #{t.id} +{f[1]}" if store.add_tag(t, f[1]) else f"#{t.id} already has +{f[1]}"]
    t.prio = int(f[1])
    return [f"#{t.id} is now P{t.prio}"]


def main():
    store = Store()
    out = []
    for line in sys.stdin.read().split("\n"):
        out.extend(handle(store, line))
    sys.stdout.write("".join(l + "\n" for l in out))


main()
