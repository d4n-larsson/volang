"""The command dispatcher: one command in, its output lines out."""

from parse import shape_ok, to_int
from teams import add_team, withdraw, rename, team_lines, add_group, grouped, group_names
from results import add_result, correct
from standings import standings, group_standings
from fixtures import fixture_lines
from stats import form_line, h2h_lines, goal_lines, streak_line, record_lines, summary_lines, knockout_lines
from fmt import table_lines

BAD = ["error: bad command"]


def table_or_empty(rows):
    if not rows:
        return ["table empty"]
    return table_lines(rows)


def table_command(league, args):
    if len(args) > 1:
        return BAD
    if len(args) == 1:
        if args[0] not in league.groups:
            return ["error: unknown group " + args[0]]
        return ["group " + args[0]] + table_or_empty(group_standings(league, args[0]))
    if not grouped(league):
        return table_or_empty(standings(league))
    out = []
    for g in group_names(league):
        out += ["group " + g] + table_or_empty(group_standings(league, g))
    return out


def team_command(league, cmd, args):
    if cmd == "team":
        return [add_team(league, args[0])] if shape_ok(args, 1, []) else BAD
    if cmd == "withdraw":
        return [withdraw(league, args[0])] if shape_ok(args, 1, []) else BAD
    if cmd == "rename":
        return [rename(league, args[0], args[1])] if shape_ok(args, 2, []) else BAD
    if cmd == "group":
        return [add_group(league, args[0], args[1:])] if len(args) >= 2 else BAD
    if cmd == "teams":
        return team_lines(league) if shape_ok(args, 0, []) else BAD
    return None


def result_command(league, cmd, args):
    if cmd == "result":
        if not shape_ok(args, 4, [1, 2]):
            return BAD
        return [add_result(league, args[0], to_int(args[1]), to_int(args[2]), args[3])]
    if cmd == "correct":
        if not shape_ok(args, 3, [0, 1, 2]):
            return BAD
        return [correct(league, to_int(args[0]), to_int(args[1]), to_int(args[2]))]
    return None


def report_command(league, cmd, args):
    if cmd == "table":
        return table_command(league, args)
    if cmd == "fixtures":
        return fixture_lines(league) if shape_ok(args, 0, []) else BAD
    if cmd == "form":
        return form_line(league, args[0]) if shape_ok(args, 1, []) else BAD
    if cmd == "h2h":
        return h2h_lines(league, args[0], args[1]) if shape_ok(args, 2, []) else BAD
    if cmd == "goals":
        return goal_lines(league) if shape_ok(args, 0, []) else BAD
    if cmd == "streak":
        return streak_line(league) if shape_ok(args, 0, []) else BAD
    if cmd == "record":
        return record_lines(league, args[0]) if shape_ok(args, 1, []) else BAD
    if cmd == "knockout":
        return knockout_lines(league) if shape_ok(args, 0, []) else BAD
    if cmd == "summary":
        return summary_lines(league) if shape_ok(args, 0, []) else BAD
    return None


def run(league, words):
    cmd = words[0]
    args = words[1:]
    for handler in (team_command, result_command, report_command):
        out = handler(league, cmd, args)
        if out is not None:
            return out
    return ["error: unknown command " + cmd]
