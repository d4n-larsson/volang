"""Channels and their members."""

from parse import Failure
from model import Channel
from users import require_user
import fmt


def require_channel(state, name):
    if name not in state.channels:
        raise Failure(f"unknown channel #{name}")
    return state.channels[name]


def require_member(channel, user_name):
    if user_name not in channel.members:
        raise Failure(f"{user_name} not in #{channel.name}")


def create_channel(state, creator, name, private):
    require_user(state, creator)
    if name in state.channels:
        raise Failure(f"channel #{name} exists")
    c = Channel(name, creator, private)
    c.invited.add(creator)
    c.members.append(creator)
    state.channels[name] = c
    kind = " (private)" if private else ""
    return [f"channel #{name} created by {creator}{kind}"]


def join(state, user_name, channel_name):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    if user_name in c.members:
        raise Failure(f"{user_name} already in #{c.name}")
    if c.private and user_name not in c.invited:
        raise Failure(f"#{c.name} is private")
    c.members.append(user_name)
    return [f"{user_name} joined #{c.name}"]


def invite(state, user_name, channel_name, guest):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    require_user(state, guest)
    if not c.private:
        raise Failure(f"#{c.name} is public")
    require_member(c, user_name)
    if guest in c.members:
        raise Failure(f"{guest} already in #{c.name}")
    c.invited.add(guest)
    return [f"{user_name} invited {guest} to #{c.name}"]


def leave(state, user_name, channel_name):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    require_member(c, user_name)
    c.members.remove(user_name)
    return [f"{user_name} left #{c.name}"]


def rename(state, user_name, channel_name, new_name):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    if c.creator != user_name:
        raise Failure(f"only {c.creator} can rename #{c.name}")
    if new_name in state.channels:
        raise Failure(f"channel #{new_name} exists")
    del state.channels[c.name]
    old = c.name
    c.name = new_name
    state.channels[new_name] = c
    for m in state.messages:
        if m.channel == old:
            m.channel = new_name
    return [f"#{old} renamed to #{new_name}"]


def members_lines(state, channel_name):
    c = require_channel(state, channel_name)
    kind = " (private)" if c.private else ""
    return [f"#{c.name} created by {c.creator}{kind}",
            "members: " + fmt.name_list(sorted(c.members))]
