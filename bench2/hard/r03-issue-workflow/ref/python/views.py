"""How issues and counts are written."""

TITLE_WIDTH = 30


def plural(n, word):
    if n == 1:
        return f"{n} {word}"
    return f"{n} {word}s"


def short_title(title):
    if len(title) > TITLE_WIDTH:
        return title[:TITLE_WIDTH - 3] + "..."
    return title


def who(name):
    return name if name != "" else "-"


def labels_text(labels):
    return ",".join(labels) if labels else "-"


def or_none(lines, message):
    return lines if lines else [message]


def issue_line(issue):
    return f"#{issue.num} [p{issue.priority}] {issue.status} {who(issue.assignee)} {short_title(issue.title)}"


def status_text(issue):
    if issue.status == "blocked":
        return f"blocked ({issue.history[-1].reason})"
    return issue.status


def show_lines(issue):
    out = [f"#{issue.num} [p{issue.priority}] {issue.title}",
           "  status: " + status_text(issue),
           "  assignee: " + who(issue.assignee),
           "  labels: " + labels_text(issue.labels),
           f"  created {issue.created}, updated {issue.updated}"]
    for h in issue.history:
        out.append(f"  [{h.tick}] {h.source} -> {h.target}" + (f" ({h.reason})" if h.reason != "" else ""))
    out.append("  " + plural(len(issue.comments), "comment"))
    for c in issue.comments:
        out.append(f"  [{c.tick}] {c.author}: {c.text}")
    return out


def event_lines(events, n):
    shown = events[max(0, len(events) - n):]
    return or_none([f"[{e.tick}] {e.message}" for e in shown], "no events")
