"""Reports about teams and matches: form, head to head, goals, streaks, records, summary."""

from results import active_matches, matches_of, involves, goals_for, goals_against, outcome
from standings import standings, find_row, build_rows
from teams import active_names, active_teams, playable_error, known_error
from fmt import match_line, signed, name_list


def form_line(league, name):
    e = playable_error(league, name)
    if e != "":
        return [e]
    recent = matches_of(league, name)[-5:]
    letters = "".join(outcome(m, name) for m in recent)
    if letters == "":
        letters = "-"
    return ["form " + name + ": " + letters]


def h2h_lines(league, a, b):
    for name in (a, b):
        e = known_error(league, name)
        if e != "":
            return [e]
    if a == b:
        return ["error: team cannot play itself"]
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


def goal_lines(league):
    games = active_matches(league)
    if not games:
        return ["no matches"]
    rows = build_rows(active_names(league), games)
    top = best_team(rows, lambda r: r.gf, False)
    tight = best_team(rows, lambda r: r.ga, True)
    biggest = games[0]
    total = 0
    for m in games:
        total += m.hg + m.ag
        if m.hg + m.ag > biggest.hg + biggest.ag:
            biggest = m
    return ["most scored: " + top.name + " " + str(top.gf),
            "fewest conceded: " + tight.name + " " + str(tight.ga),
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
    rows = standings(league)
    r = find_row(rows, name)
    return [name + ": P " + str(r.played) + ", W " + str(r.won) + ", D " + str(r.drawn) + ", L " + str(r.lost)
            + ", GF " + str(r.gf) + ", GA " + str(r.ga) + ", GD " + signed(r.gd()) + ", Pts " + str(r.points()),
            "home " + split_record(league, name, True) + ", away " + split_record(league, name, False),
            "position " + str(r.rank) + " of " + str(len(rows))]


def summary_lines(league):
    games = active_matches(league)
    withdrawn = len(league.teams) - len(active_teams(league))
    goals = sum(m.hg + m.ag for m in games)
    home = len([m for m in games if m.hg > m.ag])
    away = len([m for m in games if m.hg < m.ag])
    return ["teams " + str(len(league.teams)) + " (" + str(withdrawn) + " withdrawn), matches "
            + str(len(games)) + ", goals " + str(goals),
            "home wins " + str(home) + ", draws " + str(len(games) - home - away) + ", away wins " + str(away)]
