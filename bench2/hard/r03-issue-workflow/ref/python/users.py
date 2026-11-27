"""The people who comment on and work on issues."""
import tracker
import views


def add_user(t, name):
    if name in t.users:
        return ["error: user " + name + " exists"]
    t.users[name] = t.clock
    return [tracker.log_change(t, "user " + name + " added")]


def users_lines(t):
    out = []
    for name in sorted(t.users):
        mine = [i for i in t.issues.values() if i.assignee == name]
        live = sum(1 for i in mine if tracker.is_open(i))
        out.append(f"{name} (since {t.users[name]}): {live} open, {len(mine) - live} closed")
    return views.or_none(out, "no users")
