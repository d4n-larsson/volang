"""Reports about teams and matches: form, head to head, goals, streaks, records, summary."""

from results import active_matches, matches_of, involves, goals_for, goals_against, outcome
from standings import standings, find_row, build_rows, group_standings
from teams import active_names, active_teams, playable_error, known_error, grouped, group_of, group_names, group_active
from fixtures import remaining
from fmt import match_line, signed, name_list


def form_line(league, name):
    e = playable_error(league, name)
    if e != "":
        return [e]
    recent = matches_of(league, name)[-5:]
    letters = "".join(outcome(m, name) for m in recent)
    if letters == "":
        letters = "-"
    if group_of(league, name) != "":
        return ["form " + name + " (group " + group_of(league, name) + "): " + letters]
    return ["form " + name + ": " + letters]


def h2h_lines(league, a, b):
    for name in (a, b):
        e = known_error(league, name)
        if e != "":
            return [e]
    if a == b:
        return ["error: team cannot play itself"]
    if grouped(league) and (group_of(league, a) == "" or group_of(league, a) != group_of(league, b)):
        return ["error: " + a + " and " + b + " are not in the same group"]
    games = [m for m in active_matches(league) if involves(m, a) and involves(m, b)]
    if not games:
        return ["no matches between " + a + " and " + b]
    out = [match_line(m) for m in games]
    a_wins = len([m for m in games if outcome(m, a) == "W"])
    b_wins = len([m for m in games if outcome(m, b) == "W"])
    draws = len(games) - a_wins - b_wins
    out.append(a + " " + str(a_wins) + " wins, " + b + " " + str(b_wins) + " wins, " + str(draws) + " draws")
    return out


def best_team(rows, value, lowest):
    """The row with the highest (or lowest) value; the first name in byte order on a tie."""
    best = None
    for r in sorted(rows, key=lambda r: r.name):
        if best is None or (value(r) < value(best) if lowest else value(r) > value(best)):
            best = r
    return best


def leaders(names, games):
    """The rows with the most goals scored and the fewest conceded among the teams `names`."""
    rows = build_rows(names, games)
    return best_team(rows, lambda r: r.gf, False), best_team(rows, lambda r: r.ga, True)


def goal_lines(league):
    games = active_matches(league)
    if not games:
        return ["no matches"]
    out = []
    if grouped(league):
        for g in group_names(league):
            if group_active(league, g):
                top, tight = leaders(group_active(league, g), games)
                out.append("group " + g + ": most scored " + top.name + " " + str(top.gf)
                           + ", fewest conceded " + tight.name + " " + str(tight.ga))
    else:
        top, tight = leaders(active_names(league), games)
        out = ["most scored: " + top.name + " " + str(top.gf), "fewest conceded: " + tight.name + " " + str(tight.ga)]
    biggest = games[0]
    total = 0
    for m in games:
        total += m.hg + m.ag
        if m.hg + m.ag > biggest.hg + biggest.ag:
            biggest = m
    return out + [
            "highest scoring match: " + match_line(biggest) + " (" + str(biggest.hg + biggest.ag) + " goals)",
            "total: " + str(total) + " goals in " + str(len(games)) + " matches"]


def longest_run(league, name):
    best = 0
    run = 0
    for m in matches_of(league, name):
        if outcome(m, name) == "W":
            run += 1
            best = max(best, run)
        else:
            run = 0
    return best


def streak_line(league):
    names = sorted(active_names(league))
    best = 0
    for name in names:
        best = max(best, longest_run(league, name))
    if best == 0:
        return ["no wins yet"]
    leaders = [name for name in names if longest_run(league, name) == best]
    return ["longest winning streak " + str(best) + ": " + name_list(leaders)]


def split_record(league, name, at_home):
    w = d = l = 0
    for m in matches_of(league, name):
        if (m.home == name) != at_home:
            continue
        o = outcome(m, name)
        if o == "W":
            w += 1
        elif o == "D":
            d += 1
        else:
            l += 1
    return str(w) + "-" + str(d) + "-" + str(l)


def record_lines(league, name):
    e = playable_error(league, name)
    if e != "":
        return [e]
    g = group_of(league, name)
    if grouped(league) and g == "":
        rows = standings(league)
        where = "not in a group"
    elif grouped(league):
        rows = group_standings(league, g)
        where = None
    else:
        rows = standings(league)
        where = None
    r = find_row(rows, name)
    if where is None:
        where = "position " + str(r.rank) + " of " + str(len(rows)) + ("" if g == "" else " in group " + g)
    return [name + ": P " + str(r.played) + ", W " + str(r.won) + ", D " + str(r.drawn) + ", L " + str(r.lost)
            + ", GF " + str(r.gf) + ", GA " + str(r.ga) + ", GD " + signed(r.gd()) + ", Pts " + str(r.points()),
            "home " + split_record(league, name, True) + ", away " + split_record(league, name, False),
            where]


def summary_lines(league):
    games = active_matches(league)
    withdrawn = len(league.teams) - len(active_teams(league))
    goals = sum(m.hg + m.ag for m in games)
    home = len([m for m in games if m.hg > m.ag])
    away = len([m for m in games if m.hg < m.ag])
    return ["teams " + str(len(league.teams)) + " (" + str(withdrawn) + " withdrawn), matches "
            + str(len(games)) + ", goals " + str(goals),
            "home wins " + str(home) + ", draws " + str(len(games) - home - away) + ", away wins " + str(away)]


def knockout_lines(league):
    if not grouped(league):
        return ["error: no groups"]
    names = group_names(league)
    if len(names) < 2:
        return ["error: knockout needs at least 2 groups"]
    for g in names:
        if len(group_active(league, g)) < 2:
            return ["error: group " + g + " has fewer than 2 teams"]
        left = len(remaining(league, group_active(league, g)))
        if left > 0:
            return ["error: group " + g + " has " + str(left) + " fixtures left"]
    out = []
    for i, g in enumerate(names):
        h = names[(i + 1) % len(names)]
        first = group_standings(league, g)[0].name
        second = group_standings(league, h)[1].name
        out.append("match " + str(i + 1) + ": " + first + " (" + g + " 1st) v " + second + " (" + h + " 2nd)")
    return out
