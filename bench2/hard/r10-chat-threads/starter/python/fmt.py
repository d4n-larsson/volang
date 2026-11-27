"""Text formats shared by the views."""


def plural(n, word):
    """`1 message`, `0 messages`, `2 messages`."""
    if n == 1:
        return f"{n} {word}"
    return f"{n} {word}s"


def label(channel_name):
    return "#" + channel_name


def labels(channel_names):
    """`#a, #b`, or `no channels` for none."""
    if not channel_names:
        return "no channels"
    return ", ".join(label(n) for n in channel_names)


def name_list(names):
    if not names:
        return "none"
    return ", ".join(names)


def message_line(m):
    """`[ID] t=TIME AUTHOR: TEXT`, with ` (edited)` after an edited message."""
    line = f"[{m.msg_id}] t={m.time} {m.author}: {m.text}"
    if m.edited:
        line += " (edited)"
    return line


def located_line(m):
    """A message line that also names its channel: `#CHANNEL [ID] ...`."""
    return label(m.channel) + " " + message_line(m)
