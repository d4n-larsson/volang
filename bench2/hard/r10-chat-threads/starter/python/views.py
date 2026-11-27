"""Read-only views: history, search, top authors, statistics."""

from channels import require_channel
import fmt

DEFAULT_TOP = 3


def channel_messages(state, channel_name):
    return [m for m in state.messages if m.channel == channel_name]


def history_lines(state, channel_name, last):
    """The channel's messages, oldest first; with `last` (0: all) only the last ones."""
    c = require_channel(state, channel_name)
    msgs = channel_messages(state, c.name)
    if not msgs:
        return [f"#{c.name} has no messages"]
    if last > 0 and len(msgs) > last:
        out = [f"#{c.name}: last {last} of {fmt.plural(len(msgs), 'message')}"]
        msgs = msgs[len(msgs) - last:]
    else:
        out = [f"#{c.name}: {fmt.plural(len(msgs), 'message')}"]
    for m in msgs:
        out.append(fmt.message_line(m))
    return out


def search_lines(state, channel_name, word):
    c = require_channel(state, channel_name)
    found = [m for m in channel_messages(state, c.name) if word in m.text]
    out = [f"search #{c.name} for {word}: {len(found)} found"]
    for m in found:
        out.append("  " + fmt.message_line(m))
    return out


def author_counts(state):
    counts = {}
    for m in state.messages:
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
        count = len(channel_messages(state, name))
        out.append(f"#{name}{kind}: {fmt.plural(len(c.members), 'member')}, {fmt.plural(count, 'message')}")
    return out


def stats_line(state):
    return (f"users {len(state.users)}, channels {len(state.channels)}, "
            f"messages {len(state.messages)}, edits {state.edits}, deletes {state.deletes}")
