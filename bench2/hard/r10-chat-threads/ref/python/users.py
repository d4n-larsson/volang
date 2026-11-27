"""Users: creating them and describing them."""

from parse import Failure
from model import User, all_live
import fmt


def require_user(state, name):
    if name not in state.users:
        raise Failure(f"unknown user {name}")
    return state.users[name]


def add_user(state, name):
    if name in state.users:
        raise Failure(f"user {name} exists")
    state.users[name] = User(name)
    return [f"user {name} created"]


def channels_of(state, name):
    """The channels the user is a member of, in ascending name order."""
    return sorted(c.name for c in state.channels.values() if name in c.members)


def message_count(state, name):
    return sum(1 for _, m in all_live(state) if m.author == name)


def whois(state, name):
    require_user(state, name)
    chans = channels_of(state, name)
    if chans:
        where = "member of " + fmt.labels(chans)
    else:
        where = "member of no channels"
    return [f"{name}: {where}; {fmt.plural(message_count(state, name), 'message')}"]
