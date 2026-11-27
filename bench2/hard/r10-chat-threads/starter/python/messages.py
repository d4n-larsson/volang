"""Posting, editing and deleting messages."""

from parse import Failure
from model import Message
from users import require_user
from channels import require_channel, require_member


def find_message(state, msg_id):
    for m in state.messages:
        if m.msg_id == msg_id:
            return m
    raise Failure(f"unknown message {msg_id}")


def post(state, user_name, channel_name, text):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    require_member(c, user_name)
    m = Message(state.next_id, c.name, user_name, text, state.time)
    state.next_id += 1
    state.messages.append(m)
    return [f"message {m.msg_id} posted to #{c.name}"]


def edit(state, user_name, msg_id, text):
    require_user(state, user_name)
    m = find_message(state, msg_id)
    if m.author != user_name:
        raise Failure(f"{user_name} cannot edit message {msg_id}")
    m.text = text
    m.edited = True
    state.edits += 1
    return [f"message {msg_id} edited"]


def may_delete(state, user_name, m):
    """The author may delete a message, and so may the creator of its channel."""
    return m.author == user_name or state.channels[m.channel].creator == user_name


def delete(state, user_name, msg_id):
    require_user(state, user_name)
    m = find_message(state, msg_id)
    if not may_delete(state, user_name, m):
        raise Failure(f"{user_name} cannot delete message {msg_id}")
    state.messages.remove(m)
    state.deletes += 1
    return [f"message {msg_id} deleted"]
