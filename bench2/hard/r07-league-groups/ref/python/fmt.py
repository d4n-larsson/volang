"""Text formatting shared by every report."""


def lpad(s, width):
    while len(s) < width:
        s = " " + s
    return s


def rpad(s, width):
    while len(s) < width:
        s = s + " "
    return s


def signed(n):
    if n > 0:
        return "+" + str(n)
    return str(n)


def score(m):
    return str(m.hg) + "-" + str(m.ag)


def match_line(m):
    return "result " + str(m.num) + ": " + m.home + " " + score(m) + " " + m.away


def name_list(names):
    return ", ".join(names)


def table_lines(rows):
    """The standings table for rows that are already sorted and ranked."""
    width = 4
    for r in rows:
        width = max(width, len(r.name))
    out = ["Pos " + rpad("Team", width) + lpad("P", 3) + lpad("W", 3) + lpad("D", 3) + lpad("L", 3)
           + lpad("GF", 3) + lpad("GA", 3) + lpad("GD", 4) + lpad("Pts", 4)]
    for r in rows:
        out.append(lpad(str(r.rank), 3) + " " + rpad(r.name, width) + lpad(str(r.played), 3) + lpad(str(r.won), 3)
                   + lpad(str(r.drawn), 3) + lpad(str(r.lost), 3) + lpad(str(r.gf), 3) + lpad(str(r.ga), 3)
                   + lpad(signed(r.gf - r.ga), 4) + lpad(str(r.points()), 4))
    return out
