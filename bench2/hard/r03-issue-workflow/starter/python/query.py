"""Questions about the issues: lists, searches and counts."""
import tracker
import views


def status_matches(issue, status):
    if status == "all":
        return True
    return issue.status == status


def matches(issue, f):
    if not status_matches(issue, f.status):
        return False
    if f.assignee == "-" and issue.assignee != "":
        return False
    if f.assignee not in ("", "-") and issue.assignee != f.assignee:
        return False
    if f.label != "" and f.label not in issue.labels:
        return False
    if f.priority != 0 and issue.priority != f.priority:
        return False
    return True


def by_priority(issues):
    return sorted(issues, key=lambda i: (i.priority, i.num))


def list_lines(t, f):
    rows = [i for i in by_priority(t.issues.values()) if matches(i, f)]
    return views.or_none([views.issue_line(i) for i in rows], "no issues")


def mentions(issue, word):
    if word in issue.title.lower():
        return True
    return any(word in c.text.lower() for c in issue.comments)


def search_lines(t, word):
    w = word.lower()
    rows = [i for i in t.issues.values() if mentions(i, w)]
    return views.or_none([views.issue_line(i) for i in rows], "no matches")


def mine_lines(t, user):
    rows = [i for i in by_priority(t.issues.values()) if i.assignee == user and tracker.is_open(i)]
    return views.or_none([views.issue_line(i) for i in rows], "no open issues for " + user)


def stale_lines(t, n):
    rows = [i for i in t.issues.values() if tracker.is_open(i) and t.clock - i.updated >= n]
    rows.sort(key=lambda i: (i.updated, i.num))
    out = []
    for i in rows:
        out.append(f"#{i.num} idle {views.plural(t.clock - i.updated, 'tick')}: {views.short_title(i.title)}")
    return views.or_none(out, "no stale issues")


def open_closed(issues):
    n = sum(1 for i in issues if tracker.is_open(i))
    return f"{n} open, {len(issues) - n} closed"


def stats_lines(t):
    issues = list(t.issues.values())
    out = [f"issues {len(issues)}: {open_closed(issues)}"]
    names = sorted({i.assignee for i in issues if i.assignee != ""})
    for name in names:
        out.append(f"  {name} {open_closed([i for i in issues if i.assignee == name])}")
    nobody = [i for i in issues if i.assignee == ""]
    if nobody:
        out.append(f"  unassigned {open_closed(nobody)}")
    return out


def label_lines(t):
    counts = {}
    for i in t.issues.values():
        for label in i.labels:
            total, live = counts.get(label, (0, 0))
            counts[label] = (total + 1, live + (1 if tracker.is_open(i) else 0))
    out = []
    for label in sorted(counts):
        total, live = counts[label]
        out.append(f"{label}: {live} open of {total}")
    return views.or_none(out, "no labels")
