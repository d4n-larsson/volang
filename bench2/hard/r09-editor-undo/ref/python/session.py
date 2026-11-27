"""The editor: named buffers with their histories, the clipboard and an open undo group."""
import view
from buffer import Buffer, EditError
from history import History
from parse import positive


class Doc:
    def __init__(self):
        self.buf = Buffer()
        self.history = History()


class Editor:
    def __init__(self):
        self.docs = {"main": Doc()}
        self.current = "main"
        self.clipboard = []
        # The undo position of the current buffer when `group` ran, or None without an open group.
        self.group_start = None

    def doc(self):
        return self.docs[self.current]


def need(args, n):
    if len(args) != n:
        raise EditError("bad command")


def valid_name(name):
    return name != "" and all("a" <= c <= "z" or "0" <= c <= "9" for c in name)


def end_group(ed):
    n = ed.doc().history.merge_since(ed.group_start)
    ed.group_start = None
    return ["grouped " + view.plural(n, "change")]


def cmd_open(ed, args):
    need(args, 1)
    name = args[0]
    if not valid_name(name):
        raise EditError("bad command")
    out = end_group(ed) if ed.group_start is not None else []
    ed.current = name
    if name not in ed.docs:
        ed.docs[name] = Doc()
        return out + ["new buffer " + name]
    return out + ["buffer " + name + ", " + view.plural(ed.doc().buf.size(), "line")]


def close(ed, args, force):
    need(args, 1)
    name = args[0]
    if name not in ed.docs:
        raise EditError("no buffer " + name)
    if name == ed.current:
        raise EditError("buffer " + name + " is current")
    if not force and ed.docs[name].history.modified():
        raise EditError("buffer " + name + " has unsaved changes")
    del ed.docs[name]
    return ["closed " + name]


def cmd_buffers(ed, args):
    need(args, 0)
    out = []
    for name in sorted(ed.docs):
        d = ed.docs[name]
        mark = "*" if name == ed.current else " "
        line = mark + " " + name + " " + view.plural(d.buf.size(), "line")
        if d.history.modified():
            line += ", modified"
        out.append(line)
    return out


def cmd_where(ed, args):
    need(args, 0)
    buf = ed.doc().buf
    return ["buffer " + ed.current + ", line " + str(buf.cur) + " of " + str(buf.size())]


def no_group(ed):
    if ed.group_start is not None:
        raise EditError("group open")


def steps_count(args):
    if len(args) == 0:
        return 1
    need(args, 1)
    return positive(args[0])


def cmd_undo(ed, args):
    k = steps_count(args)
    no_group(ed)
    d = ed.doc()
    h = d.history
    if len(h.undo) == 0:
        raise EditError("nothing to undo")
    if k > len(h.undo):
        raise EditError("only " + view.plural(len(h.undo), "step") + " to undo")
    for _ in range(k):
        step = h.undo.pop()
        h.redo.append(step)
    d.buf.lines = list(step.before_lines)
    d.buf.cur = step.before_cur
    return ["undone " + view.plural(k, "step")]


def cmd_redo(ed, args):
    k = steps_count(args)
    no_group(ed)
    d = ed.doc()
    h = d.history
    if len(h.redo) == 0:
        raise EditError("nothing to redo")
    if k > len(h.redo):
        raise EditError("only " + view.plural(len(h.redo), "step") + " to redo")
    for _ in range(k):
        step = h.redo.pop()
        h.undo.append(step)
    d.buf.lines = list(step.after_lines)
    d.buf.cur = step.after_cur
    return ["redone " + view.plural(k, "step")]


def cmd_group(ed, args):
    need(args, 0)
    if ed.group_start is not None:
        raise EditError("group already open")
    ed.group_start = ed.doc().history.position()
    return ["group started"]


def cmd_endgroup(ed, args):
    need(args, 0)
    if ed.group_start is None:
        raise EditError("no group open")
    return end_group(ed)


def cmd_save(ed, args):
    need(args, 0)
    no_group(ed)
    d = ed.doc()
    d.history.saved = d.history.position()
    return ["saved " + ed.current + ", " + view.plural(d.buf.size(), "line")]


SESSION_COMMANDS = {
    "open": cmd_open,
    "close": lambda ed, args: close(ed, args, False),
    "close!": lambda ed, args: close(ed, args, True),
    "buffers": cmd_buffers,
    "where": cmd_where,
    "undo": cmd_undo,
    "redo": cmd_redo,
    "group": cmd_group,
    "endgroup": cmd_endgroup,
    "save": cmd_save,
}
