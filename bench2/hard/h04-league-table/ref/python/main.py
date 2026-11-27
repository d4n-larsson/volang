import sys


def stats(matches, teams):
    st = {t: [0, 0, 0] for t in teams}            # points, gd, gf
    for h, hg, ag, a in matches:
        if h in st and a in st:
            st[h][1] += hg - ag
            st[a][1] += ag - hg
            st[h][2] += hg
            st[a][2] += ag
            if hg > ag:
                st[h][0] += 3
            elif hg < ag:
                st[a][0] += 3
            else:
                st[h][0] += 1
                st[a][0] += 1
    return st


def order_group(group, matches, overall):
    if len(group) < 2:
        return group
    st = stats(matches, set(group))
    keys = sorted({tuple(st[t]) for t in group}, reverse=True)
    if len(keys) == 1:
        return sorted(group, key=lambda t: (-overall[t][1], -overall[t][2], t))
    out = []
    for k in keys:
        out += order_group([t for t in group if tuple(st[t]) == k], matches, overall)
    return out


def main():
    lines = [l for l in sys.stdin.read().split("\n") if l]
    teams = lines[0].split(" ")
    matches = []
    for l in lines[1:]:
        h, hg, ag, a = l.split(" ")
        matches.append((h, int(hg), int(ag), a))
    overall = stats(matches, set(teams))
    rec = {t: [0, 0, 0, 0, 0] for t in teams}     # p w d l ga
    for h, hg, ag, a in matches:
        for t, f, g in ((h, hg, ag), (a, ag, hg)):
            r = rec[t]
            r[0] += 1
            r[1 if f > g else 2 if f == g else 3] += 1
            r[4] += g
    table = []
    for p in sorted({overall[t][0] for t in teams}, reverse=True):
        table += order_group([t for t in teams if overall[t][0] == p], matches, overall)
    out = []
    for pos, t in enumerate(table, 1):
        p, w, d, l, ga = rec[t]
        pts, gd, gf = overall[t]
        gds = f"+{gd}" if gd > 0 else str(gd)
        out.append(f"{pos} {t} {p} {w} {d} {l} {gf} {ga} {gds} {pts}\n")
    sys.stdout.write("".join(out))


main()
