"""Reads one command's words and runs it."""
import issues
import parse
import query
import users
import views

BAD = ["error: bad command"]


def find(t, word):
    """(issue, "") or (None, error message)."""
    if not parse.is_number(word):
        return None, "error: bad command"
    num = int(word)
    if num not in t.issues:
        return None, f"error: unknown issue #{num}"
    return t.issues[num], ""


def on_new(t, args):
    if len(args) < 2:
        return BAD
    p = parse.parse_priority(args[0])
    if p == 0:
        return ["error: bad priority " + args[0]]
    return issues.create(t, p, " ".join(args[1:]))


def on_issue(t, args, count, at_least, action):
    """Commands whose first argument is an issue: checks the argument count, then the issue."""
    if len(args) < count or (not at_least and len(args) != count):
        return BAD
    issue, error = find(t, args[0])
    if issue is None:
        return [error]
    return action(issue, args[1:])


def on_label(t, issue, rest):
    ops = parse.parse_label_ops(rest)
    if ops is None:
        return BAD
    return issues.relabel(t, issue, ops)


def on_priority(t, issue, rest):
    p = parse.parse_priority(rest[0])
    if p == 0:
        return ["error: bad priority " + rest[0]]
    return issues.set_priority(t, issue, p)


def on_dup(t, args):
    if len(args) != 2 or not parse.is_number(args[0]) or not parse.is_number(args[1]):
        return BAD
    issue, error = find(t, args[0])
    if issue is None:
        return [error]
    original, error = find(t, args[1])
    if original is None:
        return [error]
    return issues.duplicate(t, issue, original)


def on_list(t, args):
    f, error = parse.parse_filter(args)
    if f is None:
        return [error]
    return query.list_lines(t, f)


def number_arg(args):
    """The single numeric argument, or -1."""
    if len(args) != 1 or not parse.is_number(args[0]):
        return -1
    return int(args[0])


def on_stale(t, args):
    n = number_arg(args)
    if n < 0:
        return BAD
    return query.stale_lines(t, n)


def on_wait(t, args):
    n = number_arg(args)
    if n < 1:
        return BAD
    t.clock += n - 1
    return [f"clock {t.clock}"]


def on_log(t, args):
    if len(args) == 0:
        return views.event_lines(t.events, 5)
    n = number_arg(args)
    if n < 1:
        return BAD
    return views.event_lines(t.events, n)


def on_mine(t, args):
    if len(args) != 1:
        return BAD
    if args[0] not in t.users:
        return ["error: unknown user " + args[0]]
    return query.mine_lines(t, args[0])


def no_args(args, lines):
    return BAD if len(args) != 0 else lines()


def run_issue_command(t, cmd, args):
    if cmd == "assign":
        return on_issue(t, args, 2, False, lambda i, rest: issues.assign(t, i, rest[0]))
    if cmd == "unassign":
        return on_issue(t, args, 1, False, lambda i, rest: issues.unassign(t, i))
    if cmd == "label":
        return on_issue(t, args, 2, True, lambda i, rest: on_label(t, i, rest))
    if cmd == "comment":
        return on_issue(t, args, 3, True, lambda i, rest: issues.comment(t, i, rest[0], " ".join(rest[1:])))
    if cmd == "priority":
        return on_issue(t, args, 2, False, lambda i, rest: on_priority(t, i, rest))
    if cmd == "rename":
        return on_issue(t, args, 2, True, lambda i, rest: issues.rename(t, i, " ".join(rest)))
    if cmd == "close":
        return on_issue(t, args, 1, False, lambda i, rest: issues.close(t, i))
    if cmd == "reopen":
        return on_issue(t, args, 1, False, lambda i, rest: issues.reopen(t, i))
    if cmd == "show":
        return on_issue(t, args, 1, False, lambda i, rest: views.show_lines(i))
    return None


def run(t, words):
    cmd, args = words[0], words[1:]
    out = run_issue_command(t, cmd, args)
    if out is not None:
        return out
    if cmd == "user":
        return BAD if len(args) != 1 else users.add_user(t, args[0])
    if cmd == "users":
        return no_args(args, lambda: users.users_lines(t))
    if cmd == "new":
        return on_new(t, args)
    if cmd == "dup":
        return on_dup(t, args)
    if cmd == "list":
        return on_list(t, args)
    if cmd == "search":
        return BAD if len(args) != 1 else query.search_lines(t, args[0])
    if cmd == "mine":
        return on_mine(t, args)
    if cmd == "stale":
        return on_stale(t, args)
    if cmd == "stats":
        return no_args(args, lambda: query.stats_lines(t))
    if cmd == "labels":
        return no_args(args, lambda: query.label_lines(t))
    if cmd == "log":
        return on_log(t, args)
    if cmd == "wait":
        return on_wait(t, args)
    return ["error: unknown command " + cmd]
