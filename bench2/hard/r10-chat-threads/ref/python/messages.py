"""Posting, replying, editing and deleting messages."""

from parse import Failure
from model import Message, replies
from users import require_user
from channels import require_channel, require_member


def find_message(state, channel_name, number, deleted_ok=False):
    """The message at address CHANNEL/NUMBER; a deleted thread parent only with deleted_ok."""
    c = require_channel(state, channel_name)
    m = c.messages.get(number)
    if m is None or (m.deleted and not deleted_ok):
        raise Failure(f"unknown message {channel_name}/{number}")
    return c, m


def add_message(state, c, user_name, text, parent):
    m = Message(c.next_number, user_name, text, state.time, parent)
    c.next_number += 1
    c.messages[m.msg_id] = m
    c.markers[user_name] = state.time
    return m


def post(state, user_name, channel_name, text):
    require_user(state, user_name)
    c = require_channel(state, channel_name)
    require_member(c, user_name)
    m = add_message(state, c, user_name, text, 0)
    return [f"message {m.msg_id} posted to #{c.name}"]


def reply(state, user_name, channel_name, number, text):
    require_user(state, user_name)
    c, target = find_message(state, channel_name, number)
    require_member(c, user_name)
    root = target.parent or target.msg_id
    m = add_message(state, c, user_name, text, root)
    return [f"message {m.msg_id} posted to #{c.name} in thread {root}"]


def edit(state, user_name, channel_name, number, text):
    require_user(state, user_name)
    c, m = find_message(state, channel_name, number)
    if m.author != user_name:
        raise Failure(f"{user_name} cannot edit message {c.name}/{number}")
    m.text = text
    m.edited = True
    state.edits += 1
    return [f"message {c.name}/{number} edited"]


def may_delete(c, user_name, m):
    """The author may delete a message, and so may the creator of its channel."""
    return m.author == user_name or c.creator == user_name


def delete(state, user_name, channel_name, number):
    require_user(state, user_name)
    c, m = find_message(state, channel_name, number)
    if not may_delete(c, user_name, m):
        raise Failure(f"{user_name} cannot delete message {c.name}/{number}")
    if m.parent:
        del c.messages[number]
        parent = c.messages[m.parent]
        if parent.deleted and not replies(c, parent.msg_id):
            del c.messages[parent.msg_id]
    elif replies(c, number):
        m.deleted = True
    else:
        del c.messages[number]
    state.deletes += 1
    return [f"message {c.name}/{number} deleted"]
