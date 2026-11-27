"""Changes to issues. Each returns the command's output lines."""
import tracker
import views


def create(t, priority, title):
    num = t.next_num
    t.next_num += 1
    t.issues[num] = tracker.Issue(num, title, priority, t.clock)
    return [tracker.log_change(t, f"created #{num}: {title}")]


def assign(t, issue, user):
    if not tracker.is_open(issue):
        return [f"error: issue #{issue.num} is closed"]
    if user not in t.users:
        return ["error: unknown user " + user]
    if issue.assignee == user:
        return [f"error: #{issue.num} is already assigned to {user}"]
    before = issue.assignee
    issue.assignee = user
    tracker.touch(t, issue)
    if before != "":
        return [tracker.log_change(t, f"#{issue.num} reassigned from {before} to {user}")]
    return [tracker.log_change(t, f"#{issue.num} assigned to {user}")]


def unassign(t, issue):
    if issue.assignee == "":
        return [f"error: issue #{issue.num} has no assignee"]
    issue.assignee = ""
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} unassigned")]


def relabel(t, issue, ops):
    for adding, label in ops:
        if adding and label not in issue.labels:
            issue.labels.append(label)
        elif not adding and label in issue.labels:
            issue.labels.remove(label)
    issue.labels.sort()
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} labels: {views.labels_text(issue.labels)}")]


def comment(t, issue, author, text):
    if author not in t.users:
        return ["error: unknown user " + author]
    issue.comments.append(tracker.Comment(t.clock, author, text))
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"comment added to #{issue.num}")]


def set_priority(t, issue, priority):
    if issue.priority == priority:
        return [f"#{issue.num} priority unchanged"]
    old = issue.priority
    issue.priority = priority
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} priority p{old} -> p{priority}")]


def rename(t, issue, title):
    if issue.title == title:
        return [f"#{issue.num} title unchanged"]
    issue.title = title
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} renamed")]


def close(t, issue):
    if issue.status == "closed":
        return [f"error: issue #{issue.num} is already closed"]
    issue.status = "closed"
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} closed")]


def reopen(t, issue):
    if issue.status != "closed":
        return [f"error: issue #{issue.num} is not closed"]
    issue.status = "open"
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} reopened")]


def duplicate(t, issue, original):
    """Closes `issue` as a duplicate of `original`."""
    if issue.num == original.num:
        return [f"error: issue #{issue.num} cannot duplicate itself"]
    if issue.status == "closed":
        return [f"error: issue #{issue.num} is already closed"]
    if not tracker.is_open(original):
        return [f"error: issue #{original.num} is closed"]
    issue.status = "closed"
    if "duplicate" not in issue.labels:
        issue.labels.append("duplicate")
        issue.labels.sort()
    issue.comments.append(tracker.Comment(t.clock, "system", f"duplicate of #{original.num}"))
    tracker.touch(t, issue)
    return [tracker.log_change(t, f"#{issue.num} closed as duplicate of #{original.num}")]
