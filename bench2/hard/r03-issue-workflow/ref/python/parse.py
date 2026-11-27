"""Checks and conversions of command words."""


class Filter:
    def __init__(self):
        self.status = "open"      # "open", "closed", "all" or a state
        self.assignee = ""        # "" for anyone, "-" for unassigned issues
        self.label = ""           # "" for any labels
        self.priority = 0         # 0 for any priority


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def parse_priority(s):
    """1 to 4 for p1 to p4, else 0."""
    if len(s) == 2 and s[0] == "p" and s[1] in "1234":
        return int(s[1])
    return 0


def parse_label_ops(words):
    """[(adding, label)] for words +LABEL / -LABEL, or None if a word is neither."""
    ops = []
    for w in words:
        if len(w) < 2 or w[0] not in "+-":
            return None
        ops.append((w[0] == "+", w[1:]))
    return ops


STATUS_WORDS = ["open", "closed", "all", "new", "triaged", "in_progress", "blocked", "review", "done", "wontfix"]


def parse_filter(words):
    """(Filter, "") or (None, error message)."""
    f = Filter()
    seen = []
    for w in words:
        eq = w.find("=")
        if eq <= 0 or eq == len(w) - 1:
            return None, "error: bad filter " + w
        key, value = w[:eq], w[eq + 1:]
        if key in seen:
            return None, "error: duplicate filter " + key
        seen.append(key)
        if key == "status":
            if value not in STATUS_WORDS:
                return None, "error: bad status " + value
            f.status = value
        elif key == "assignee":
            f.assignee = value
        elif key == "label":
            f.label = value
        elif key == "priority":
            p = parse_priority(value)
            if p == 0:
                return None, "error: bad priority " + value
            f.priority = p
        else:
            return None, "error: unknown filter " + key
    return f, ""
