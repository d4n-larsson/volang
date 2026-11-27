"""Commands that add, remove and rearrange lines."""
from buffer import EditError


def append(buf, text):
    buf.lines.append(text)
    buf.cur = buf.size()
    return []


def insert(buf, at, text):
    buf.put_block(at, at - 1, [text])
    buf.cur = at
    return []


def delete(buf, a, b):
    buf.put_block(a, b, [])
    buf.cur = a
    buf.clamp_cur()
    return []


def replace(buf, at, text):
    buf.put_block(at, at, [text])
    buf.cur = at
    return []


def clear(buf):
    buf.lines = []
    buf.cur = 0
    return []


def move(buf, a, b, after):
    """Moves lines a to b after line `after` (numbered before the move)."""
    if a <= after < b:
        raise EditError("cannot move lines into themselves")
    block = buf.block(a, b)
    buf.put_block(a, b, [])
    at = after if after < a else after - len(block)
    buf.put_block(at + 1, at, block)
    buf.cur = at + len(block)
    return []


def dup(buf, a, b):
    """Copies lines a to b right after line b."""
    block = buf.block(a, b)
    buf.put_block(b + 1, b, block)
    buf.cur = b + len(block)
    return []


def join_lines(buf, a, b):
    if a == b:
        raise EditError("nothing to join")
    buf.put_block(a, b, [" ".join(buf.block(a, b))])
    buf.cur = a
    return []


def split_at(buf, at, col):
    text = buf.line(at)
    if col < 1 or col >= len(text):
        raise EditError("bad column " + str(col))
    buf.put_block(at, at, [text[:col], text[col:]])
    buf.cur = at + 1
    return []
