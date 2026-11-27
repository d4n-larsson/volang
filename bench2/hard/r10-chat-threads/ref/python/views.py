"""Read-only views: history, threads, search, top authors, statistics."""

from channels import require_channel
from messages import find_message
from model import live_messages, replies, all_live
import fmt

DEFAULT_TOP = 3


def history_lines(state, channel_name, last):
    """The channel's top-level messages, oldest first; with `last` (0: all) only the last ones."""
    c = require_channel(state, channel_name)
    msgs = [m for m in c.messages.values() if m.parent == 0]
    if not msgs:
        return [f"#{c.name} has no messages"]
    if last > 0 and len(msgs) > last:
        out = [f"#{c.name}: last {last} of {fmt.plural(len(msgs), 'message')}"]
        msgs = msgs[len(msgs) - last:]
    else:
        out = [f"#{c.name}: {fmt.plural(len(msgs), 'message')}"]
    for m in msgs:
        line = fmt.message_line(m)
        n = len(replies(c, m.msg_id))
        if n:
            line += f" ({fmt.replies_text(n)})"
        out.append(line)
    return out


def thread_lines(state, channel_name, number):
    c, m = find_message(state, channel_name, number, deleted_ok=True)
    root = c.messages[m.parent] if m.parent else m
    rs = replies(c, root.msg_id)
    out = [f"thread #{c.name}/{root.msg_id}: {fmt.replies_text(len(rs))}", fmt.message_line(root)]
    for r in rs:
        out.append("  " + fmt.message_line(r))
    return out


def search_lines(state, channel_name, word):
    c = require_channel(state, channel_name)
    found = [m for m in live_messages(c) if word in m.text]
    out = [f"search #{c.name} for {word}: {len(found)} found"]
    for m in found:
        out.append("  " + fmt.found_line(m))
    return out


def author_counts(state):
    counts = {}
    for _, m in all_live(state):
        counts[m.author] = counts.get(m.author, 0) + 1
    return counts


def top_lines(state, n):
    """The n most active authors: most messages first, then by name."""
    counts = author_counts(state)
    if not counts:
        return ["no messages"]
    ranked = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))
    out = []
    for i, (name, count) in enumerate(ranked[:n]):
        out.append(f"{i + 1}. {name} {fmt.plural(count, 'message')}")
    return out


def channel_list_lines(state):
    """Every channel in ascending name order with its member and message counts."""
    if not state.channels:
        return ["no channels"]
    out = []
    for name in sorted(state.channels):
        c = state.channels[name]
        kind = " (private)" if c.private else ""
        count = len(live_messages(c))
        out.append(f"#{name}{kind}: {fmt.plural(len(c.members), 'member')}, {fmt.plural(count, 'message')}")
    return out


def stats_line(state):
    return (f"users {len(state.users)}, channels {len(state.channels)}, "
            f"messages {len(all_live(state))}, edits {state.edits}, deletes {state.deletes}")
