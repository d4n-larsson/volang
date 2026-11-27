"""Pagination: placing blocks on pages under the keep rules, headers and footers."""


class Block:
    def __init__(self, kind, lines, lst, brk, title, space, text=""):
        self.kind = kind          # "para" or "heading"
        self.lines = lines
        self.lst = lst            # the List object of an item, else None
        self.brk = brk            # a .break came before this block
        self.title = title
        self.space = space        # separator lines set by .space, or None
        self.text = text          # a heading's shown text
        self.page = 0             # the page of a heading's first line


def sep(prev, b):
    if b.space is not None:
        return b.space
    if prev is not None and prev.lst is not None and prev.lst is b.lst:
        return 0
    return 1


def need(blocks, i):
    b = blocks[i]
    if b.kind == "para":
        n = len(b.lines)
        return n if n <= 3 else 2
    if i + 1 == len(blocks) or blocks[i + 1].brk:
        return 2
    return 2 + sep(b, blocks[i + 1]) + need(blocks, i + 1)


def paginate(blocks, h):
    """Pages, each [title, body lines]."""
    pages = []
    cur = None
    prev = None

    def new_page(title):
        nonlocal cur, prev
        cur = [title, []]
        pages.append(cur)
        prev = None

    for i, b in enumerate(blocks):
        if cur is not None and cur[1] and (b.brk or len(cur[1]) + sep(prev, b) + need(blocks, i) > h):
            cur = None
        if cur is None:
            new_page(b.title)
        rest = b.lines
        b.page = len(pages)
        while rest:
            used = len(cur[1])
            if used:
                s = sep(prev, b)
                room = h - used - s
                k = len(rest) if len(rest) <= room or b.kind == "heading" else min(room, len(rest) - 2)
                cur[1].extend([""] * s)
            else:
                k = len(rest) if len(rest) <= h or b.kind == "heading" else min(h, len(rest) - 2)
            cur[1].extend(rest[:k])
            rest = rest[k:]
            prev = b
            if rest:
                new_page(b.title)
        prev = b
    return pages


def centered(text, w):
    text = text[:w]
    return (" " * ((w - len(text)) // 2) + text).rstrip(" ")


def contents(blocks, w):
    out = ["CONTENTS"]
    for b in blocks:
        if b.kind == "heading":
            page = str(b.page)
            text = b.text[:w - 5 - len(page)]
            out.append(text + " " + "." * (w - len(text) - 2 - len(page)) + " " + page)
    return out


def render(pages, w, h):
    out = []
    for p, (title, body) in enumerate(pages, 1):
        out.append(centered(title, w))
        out.append("")
        out.extend(body)
        if p < len(pages):
            out.extend([""] * (h - len(body)))
        out.append("")
        out.append(centered(f"page {p} of {len(pages)}", w))
    return out
