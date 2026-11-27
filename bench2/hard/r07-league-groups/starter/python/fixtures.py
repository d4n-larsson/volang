"""The double round robin: every active team plays every other active team at home and away."""

from results import played
from teams import active_names


def remaining(league, names):
    """(home, away) pairs not yet played among `names`, home then away in the order of `names`."""
    out = []
    for home in names:
        for away in names:
            if home != away and not played(league, home, away):
                out.append((home, away))
    return out


def fixture_lines(league):
    pairs = remaining(league, active_names(league))
    out = [home + " v " + away for home, away in pairs]
    out.append(str(len(pairs)) + " fixtures left")
    return out
