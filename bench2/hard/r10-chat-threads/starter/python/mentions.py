"""Mentions: a word `@NAME` in a message's text, possibly followed by punctuation."""

from users import require_user
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
    for m in state.messages:
        if user_name in state.channels[m.channel].members and mentions(m.text, user_name):
            found.append(m)
    out = [f"mentions of {user_name}: {len(found)}"]
    for m in found:
        out.append("  " + fmt.located_line(m))
    return out
