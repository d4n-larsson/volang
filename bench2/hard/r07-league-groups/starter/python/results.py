"""Entering and correcting results, and reading them back."""

from model import Match
from teams import known_error, playable_error
from fmt import match_line, score


def active_matches(league):
    return [m for m in league.matches if not m.voided]


def involves(m, name):
    return m.home == name or m.away == name


def matches_of(league, name):
    return [m for m in active_matches(league) if involves(m, name)]


def played(league, home, away):
    for m in active_matches(league):
        if m.home == home and m.away == away:
            return True
    return False


def goals_for(m, name):
    if m.home == name:
        return m.hg
    return m.ag


def goals_against(m, name):
    if m.home == name:
        return m.ag
    return m.hg


def outcome(m, name):
    """"W", "D" or "L" for the team `name` in the match."""
    gf = goals_for(m, name)
    ga = goals_against(m, name)
    if gf > ga:
        return "W"
    if gf == ga:
        return "D"
    return "L"


def add_result(league, home, hg, ag, away):
    for name in (home, away):
        e = known_error(league, name)
        if e != "":
            return e
    if home == away:
        return "error: team cannot play itself"
    for name in (home, away):
        e = playable_error(league, name)
        if e != "":
            return e
    if played(league, home, away):
        return "error: " + home + " v " + away + " already played"
    m = Match(len(league.matches) + 1, home, away, hg, ag)
    league.matches.append(m)
    return match_line(m)


def correct(league, num, hg, ag):
    if num < 1 or num > len(league.matches):
        return "error: no result " + str(num)
    m = league.matches[num - 1]
    if m.voided:
        return "error: result " + str(num) + " void"
    was = score(m)
    m.hg = hg
    m.ag = ag
    return match_line(m) + " (was " + was + ")"
