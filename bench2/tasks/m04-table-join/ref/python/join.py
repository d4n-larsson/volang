def join(kind, key, ha, ra, hb, rb):
    """(header, rows) of the joined table; ha/hb contain key."""
    ka, kb = ha.index(key), hb.index(key)
    oa = [i for i in range(len(ha)) if i != ka]
    ob = [i for i in range(len(hb)) if i != kb]
    shared = {ha[i] for i in oa} & {hb[i] for i in ob}
    header = [key] + [("a." if ha[i] in shared else "") + ha[i] for i in oa] \
        + [("b." if hb[i] in shared else "") + hb[i] for i in ob]
    rows, used = [], [False] * len(rb)
    for a in ra:
        found = False
        for j, b in enumerate(rb):
            if a[ka] != "" and a[ka] == b[kb]:
                found = used[j] = True
                rows.append([a[ka]] + [a[i] for i in oa] + [b[i] for i in ob])
        if not found and kind != "inner":
            rows.append([a[ka]] + [a[i] for i in oa] + [""] * len(ob))
    if kind == "full":
        for j, b in enumerate(rb):
            if not used[j]:
                rows.append([b[kb]] + [""] * len(oa) + [b[i] for i in ob])
    return header, rows
