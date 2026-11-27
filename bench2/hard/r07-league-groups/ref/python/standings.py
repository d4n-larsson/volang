"""The league table: one row per active team, sorted and ranked."""

from results import active_matches, goals_for, goals_against, outcome, involves
from teams import active_names, group_active


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
        # the same counts among the teams level on points (the head-to-head mini-table)
        self.mini = None

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


def place_key(row):
    return (-row.points(), -row.mini.points(), -row.mini.gd(), -row.mini.gf, -row.gd(), -row.gf)


def sort_key(row):
    return place_key(row) + (row.name,)


def same_place(a, b):
    """Rows that share a position: equal on everything but the name."""
    return place_key(a) == place_key(b)


def add_mini(rows, matches):
    """Each row's mini-table counts: the matches among the teams level with it on points."""
    for r in rows:
        level = [x.name for x in rows if x.points() == r.points()]
        among = [m for m in matches if m.home in level and m.away in level]
        r.mini = build_rows([r.name], among)[0]


def rank_rows(rows, matches):
    add_mini(rows, matches)
    rows.sort(key=sort_key)
    for i, r in enumerate(rows):
        if i > 0 and same_place(rows[i - 1], r):
            r.rank = rows[i - 1].rank
        else:
            r.rank = i + 1
    return rows


def table_for(names, league):
    matches = active_matches(league)
    return rank_rows(build_rows(names, matches), matches)


def standings(league):
    return table_for(active_names(league), league)


def group_standings(league, g):
    return table_for(group_active(league, g), league)


def find_row(rows, name):
    for r in rows:
        if r.name == name:
            return r
    return None
