"""Mentions: a word `@NAME` in a message's text, possibly followed by punctuation."""

from users import require_user
from model import all_live
import fmt

TRAILING = ".,:;!?"


def mentions(text, user_name):
    for w in text.split(" "):
        while w != "" and w[-1] in TRAILING:
            w = w[:-1]
        if w == "@" + user_name:
            return True
    return False


def mention_lines(state, user_name):
    require_user(state, user_name)
    found = []
    for c, m in all_live(state):
        if user_name in c.members and mentions(m.text, user_name):
            found.append((c, m))
    out = [f"mentions of {user_name}: {len(found)}"]
    for c, m in found:
        out.append("  " + fmt.located_line(c.name, m))
    return out


def unread_lines(state, user_name):
    """Per channel of the user: messages by others posted after the user's read marker."""
    require_user(state, user_name)
    out = []
    for name in sorted(state.channels):
        c = state.channels[name]
        if user_name not in c.members:
            continue
        marker = c.markers[user_name]
        news = [m for m in c.messages.values()
                if not m.deleted and m.author != user_name and m.time > marker]
        if not news:
            continue
        line = f"#{name}: {len(news)} unread"
        named = sum(1 for m in news if mentions(m.text, user_name))
        if named:
            line += f" ({named} mentioning you)"
        out.append(line)
    if not out:
        return [f"no unread messages for {user_name}"]
    return out
