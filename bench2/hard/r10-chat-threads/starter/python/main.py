import sys

from parse import Failure, split_words, expect_count, name_arg, number_arg, text_arg, flag_arg
from model import State
import users
import channels
import messages
import mentions
import views


def cmd_user(state, args):
    expect_count(args, 1, 1)
    return users.add_user(state, name_arg(args[0]))


def cmd_channel(state, args):
    expect_count(args, 2, 3)
    private = len(args) == 3 and flag_arg(args[2], "private")
    return channels.create_channel(state, name_arg(args[0]), name_arg(args[1]), private)


def cmd_join(state, args):
    expect_count(args, 2, 2)
    return channels.join(state, name_arg(args[0]), name_arg(args[1]))


def cmd_invite(state, args):
    expect_count(args, 3, 3)
    return channels.invite(state, name_arg(args[0]), name_arg(args[1]), name_arg(args[2]))


def cmd_leave(state, args):
    expect_count(args, 2, 2)
    return channels.leave(state, name_arg(args[0]), name_arg(args[1]))


def cmd_rename(state, args):
    expect_count(args, 3, 3)
    return channels.rename(state, name_arg(args[0]), name_arg(args[1]), name_arg(args[2]))


def cmd_post(state, args):
    expect_count(args, 3, -1)
    return messages.post(state, name_arg(args[0]), name_arg(args[1]), text_arg(args[2:]))


def cmd_edit(state, args):
    expect_count(args, 3, -1)
    return messages.edit(state, name_arg(args[0]), number_arg(args[1], 1), text_arg(args[2:]))


def cmd_delete(state, args):
    expect_count(args, 2, 2)
    return messages.delete(state, name_arg(args[0]), number_arg(args[1], 1))


def cmd_history(state, args):
    expect_count(args, 1, 2)
    last = number_arg(args[1], 1) if len(args) == 2 else 0
    return views.history_lines(state, name_arg(args[0]), last)


def cmd_search(state, args):
    expect_count(args, 2, 2)
    return views.search_lines(state, name_arg(args[0]), args[1])


def cmd_mentions(state, args):
    expect_count(args, 1, 1)
    return mentions.mention_lines(state, name_arg(args[0]))


def cmd_members(state, args):
    expect_count(args, 1, 1)
    return channels.members_lines(state, name_arg(args[0]))


def cmd_whois(state, args):
    expect_count(args, 1, 1)
    return users.whois(state, name_arg(args[0]))


def cmd_top(state, args):
    expect_count(args, 0, 1)
    n = number_arg(args[0], 1) if len(args) == 1 else views.DEFAULT_TOP
    return views.top_lines(state, n)


def cmd_channels(state, args):
    expect_count(args, 0, 0)
    return views.channel_list_lines(state)


def cmd_stats(state, args):
    expect_count(args, 0, 0)
    return [views.stats_line(state)]


COMMANDS = {
    "user": cmd_user,
    "channel": cmd_channel,
    "join": cmd_join,
    "invite": cmd_invite,
    "leave": cmd_leave,
    "rename": cmd_rename,
    "post": cmd_post,
    "edit": cmd_edit,
    "delete": cmd_delete,
    "history": cmd_history,
    "search": cmd_search,
    "mentions": cmd_mentions,
    "members": cmd_members,
    "whois": cmd_whois,
    "top": cmd_top,
    "channels": cmd_channels,
    "stats": cmd_stats,
}


def run(state, words):
    cmd = words[0]
    if cmd not in COMMANDS:
        return [f"error: unknown command {cmd}"]
    try:
        return COMMANDS[cmd](state, words[1:])
    except Failure as e:
        return [f"error: {e}"]


def main():
    state = State()
    for line in sys.stdin.read().split("\n"):
        words = split_words(line)
        if not words:
            continue
        state.time += 1
        for out in run(state, words):
            print(out)


main()
