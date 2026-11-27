import sys

import edit
import gantt
import report
from model import Plan
from parse import is_number, is_percent, parse_dep, valid_id


def bad():
    return ["error: bad command"]


def change(plan, cmd, args):
    """The commands that edit the plan, or None for any other command."""
    if cmd == "task":
        if len(args) < 2 or not is_number(args[1]):
            return bad()
        if not valid_id(args[0]):
            return ["error: bad task id " + args[0]]
        name = " ".join(args[2:]) if len(args) > 2 else args[0]
        return [edit.add_task(plan, args[0], int(args[1]), name)]
    if cmd == "rename":
        if len(args) < 2:
            return bad()
        return [edit.rename(plan, args[0], " ".join(args[1:]))]
    if cmd == "dep":
        if len(args) != 2 or parse_dep(args[1]) is None:
            return bad()
        pred, lag = parse_dep(args[1])
        return [edit.add_dep(plan, args[0], pred, lag)]
    if cmd == "undep":
        if len(args) != 2:
            return bad()
        return [edit.remove_dep(plan, args[0], args[1])]
    if cmd == "remove":
        if len(args) != 1:
            return bad()
        return [edit.remove_task(plan, args[0])]
    if cmd == "duration":
        if len(args) != 2 or not is_number(args[1]):
            return bad()
        return [edit.set_duration(plan, args[0], int(args[1]))]
    if cmd == "notbefore":
        if len(args) != 2 or not is_number(args[1]):
            return bad()
        return [edit.set_not_before(plan, args[0], int(args[1]))]
    if cmd == "progress":
        if len(args) != 2 or not is_percent(args[1]):
            return bad()
        return [edit.set_progress(plan, args[0], int(args[1]))]
    return None


def view(plan, cmd, args):
    if cmd == "list":
        if len(args) != 0:
            return bad()
        return report.list_lines(plan)
    if cmd == "show":
        if len(args) != 1:
            return bad()
        if args[0] not in plan.tasks:
            return [edit.unknown(args[0])]
        return report.show_lines(plan, args[0])
    if cmd == "schedule":
        if len(args) != 0:
            return bad()
        return report.schedule_lines(plan)
    if cmd == "critical":
        if len(args) != 0:
            return bad()
        return [report.critical_line(plan)]
    if cmd == "gantt":
        if len(args) not in (0, 2) or not all(is_number(a) for a in args):
            return bad()
        return gantt.gantt(plan, args)
    if cmd == "status":
        if len(args) != 1 or not is_number(args[0]):
            return bad()
        return report.status_lines(plan, int(args[0]))
    if cmd == "end":
        if len(args) != 0:
            return bad()
        return [report.end_line(plan)]
    return ["error: unknown command " + cmd]


def run(plan, words):
    out = change(plan, words[0], words[1:])
    if out is None:
        out = view(plan, words[0], words[1:])
    return out


def main():
    plan = Plan()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if len(words) == 0:
            continue
        for out in run(plan, words):
            print(out)


main()
