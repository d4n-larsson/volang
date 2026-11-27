"""The league table: one row per active team, sorted and ranked."""

from results import active_matches, goals_for, goals_against, outcome, involves
from teams import active_names


class Row:
    def __init__(self, name):
        self.name = name
        self.played = 0
        self.won = 0
        self.drawn = 0
        self.lost = 0
        self.gf = 0
        self.ga = 0
        self.rank = 0

    def points(self):
        return 3 * self.won + self.drawn

    def gd(self):
        return self.gf - self.ga


def tally(row, m):
    row.played += 1
    row.gf += goals_for(m, row.name)
    row.ga += goals_against(m, row.name)
    o = outcome(m, row.name)
    if o == "W":
        row.won += 1
    elif o == "D":
        row.drawn += 1
    else:
        row.lost += 1


def build_rows(names, matches):
    """Unsorted rows for the teams `names`, counting the matches among `matches` they played in."""
    rows = []
    for name in names:
        row = Row(name)
        for m in matches:
            if involves(m, name):
                tally(row, m)
        rows.append(row)
    return rows


def sort_key(row):
    return (-row.points(), -row.gd(), -row.gf, row.name)


def same_place(a, b):
    """Rows that share a position: equal on everything but the name."""
    return a.points() == b.points() and a.gd() == b.gd() and a.gf == b.gf


def rank_rows(rows):
    rows.sort(key=sort_key)
    for i, r in enumerate(rows):
        if i > 0 and same_place(rows[i - 1], r):
            r.rank = rows[i - 1].rank
        else:
            r.rank = i + 1
    return rows


def standings(league):
    return rank_rows(build_rows(active_names(league), active_matches(league)))


def find_row(rows, name):
    for r in rows:
        if r.name == name:
            return r
    return None
