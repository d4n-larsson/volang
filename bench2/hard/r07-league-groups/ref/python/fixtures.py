"""The double round robin: every active team plays every other active team at home and away."""

from results import played
from teams import active_names, grouped, group_names, group_active


def remaining(league, names):
    """(home, away) pairs not yet played among `names`, home then away in the order of `names`."""
    out = []
    for home in names:
        for away in names:
            if home != away and not played(league, home, away):
                out.append((home, away))
    return out


def group_fixture_lines(league):
    out = []
    total = 0
    for g in group_names(league):
        pairs = remaining(league, group_active(league, g))
        total += len(pairs)
        out.append("group " + g + ": " + str(len(pairs)) + " left")
        out += ["  " + home + " v " + away for home, away in pairs]
    out.append(str(total) + " fixtures left")
    return out


def fixture_lines(league):
    if grouped(league):
        return group_fixture_lines(league)
    pairs = remaining(league, active_names(league))
    out = [home + " v " + away for home, away in pairs]
    out.append(str(len(pairs)) + " fixtures left")
    return out
