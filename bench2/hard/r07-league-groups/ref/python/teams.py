"""Registering, finding, renaming and withdrawing teams."""

from model import Team


def find_team(league, name):
    for t in league.teams:
        if t.name == name:
            return t
    return None


def active_teams(league):
    return [t for t in league.teams if not t.withdrawn]


def active_names(league):
    return [t.name for t in active_teams(league)]


def known_error(league, name):
    """The error for a name that is not a team, or ""."""
    if find_team(league, name) is None:
        return "error: unknown team " + name
    return ""


def playable_error(league, name):
    """The error for a name that is not a team or is a withdrawn one, or ""."""
    e = known_error(league, name)
    if e != "":
        return e
    if find_team(league, name).withdrawn:
        return "error: team " + name + " withdrawn"
    return ""


def add_team(league, name):
    if find_team(league, name) is not None:
        return "error: team " + name + " exists"
    league.teams.append(Team(name))
    return "team " + name + " added"


def withdraw(league, name):
    e = known_error(league, name)
    if e != "":
        return e
    t = find_team(league, name)
    if t.withdrawn:
        return "error: team " + name + " already withdrawn"
    t.withdrawn = True
    count = 0
    for m in league.matches:
        if not m.voided and (m.home == name or m.away == name):
            m.voided = True
            count += 1
    return "team " + name + " withdrawn, " + str(count) + " results void"


def rename(league, old, new):
    e = known_error(league, old)
    if e != "":
        return e
    if find_team(league, new) is not None:
        return "error: team " + new + " exists"
    find_team(league, old).name = new
    for members in league.groups.values():
        for i in range(len(members)):
            if members[i] == old:
                members[i] = new
    for m in league.matches:
        if m.home == old:
            m.home = new
        if m.away == old:
            m.away = new
    return "team " + old + " renamed " + new


def grouped(league):
    return len(league.groups) > 0


def group_of(league, name):
    """The name of the team's group, or ""."""
    for g, members in league.groups.items():
        if name in members:
            return g
    return ""


def group_names(league):
    return sorted(league.groups)


def group_active(league, g):
    return [n for n in league.groups[g] if not find_team(league, n).withdrawn]


def add_group(league, g, names):
    if league.matches:
        return "error: groups are fixed once results exist"
    seen = []
    for name in names:
        e = playable_error(league, name)
        if e != "":
            return e
        if group_of(league, name) != "":
            return "error: team " + name + " already in group " + group_of(league, name)
        if name in seen:
            return "error: team " + name + " listed twice"
        seen.append(name)
    if g not in league.groups:
        league.groups[g] = []
    league.groups[g].extend(names)
    return "group " + g + ": " + ", ".join(league.groups[g])


def team_lines(league):
    out = []
    for t in league.teams:
        notes = []
        if group_of(league, t.name) != "":
            notes.append("group " + group_of(league, t.name))
        if t.withdrawn:
            notes.append("withdrawn")
        if notes:
            out.append(t.name + " (" + ", ".join(notes) + ")")
        else:
            out.append(t.name)
    out.append(str(len(league.teams)) + " teams")
    return out
