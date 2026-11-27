"""The league's data: teams in registration order and every result ever entered."""


class Team:
    def __init__(self, name):
        self.name = name
        self.withdrawn = False


class Match:
    """One result. `num` is its 1-based position in the order results were entered.
    A void result (one of its teams withdrew) no longer counts anywhere."""

    def __init__(self, num, home, away, hg, ag):
        self.num = num
        self.home = home
        self.away = away
        self.hg = hg
        self.ag = ag
        self.voided = False


class League:
    def __init__(self):
        self.teams = []
        self.matches = []
