LETTER_ORDER = ["A+", "A", "A-", "B+", "B", "B-", "C+", "C", "C-", "D+", "D", "D-", "F"]


def letter(avg):
    """The letter grade, with its + or - modifier, for an average in tenths of a percent."""
    for base, floor in (("A", 900), ("B", 800), ("C", 700), ("D", 600)):
        if avg >= floor:
            if avg - floor >= 70:
                return base + "+"
            if avg - floor < 30:
                return base + "-"
            return base
    return "F"
