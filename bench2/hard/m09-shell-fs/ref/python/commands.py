"""One function per command; each returns output lines (a file's text may span several)."""
from fstree import Entry, FsError
import listing


def each_path(cmd, paths, f):
    out = []
    for p in paths:
        try:
            out += f(p)
        except FsError as e:
            out.append(f"{cmd}: {p}: {e}")
    return out


def pwd(t, flag, args):
    return [t.abspath(t.cwd)]


def cd(t, flag, args):
    e = t.resolve(args[0])
    if not e.is_dir:
        raise FsError("not a directory")
    t.cwd = e
    return []


def mkdir_one(t, p):
    d, name, ex = t.locate(p)
    if ex is not None:
        raise FsError("file exists")
    t.add(d, name, Entry(name, d, True))
    return []


def mkdir_p_one(t, p):
    comps = t.components(p)
    cur = t.start(p)
    for i, c in enumerate(comps):
        if not cur.is_dir:
            raise FsError("not a directory")
        if c not in (".", "..") and c not in cur.children:
            t.add(cur, c, Entry(c, cur, True))
        cur = t.step(cur, c)
        if i == len(comps) - 1 and not cur.is_dir:
            raise FsError("file exists")
    return []


def mkdir(t, flag, args):
    f = mkdir_p_one if flag else mkdir_one
    return each_path("mkdir", args, lambda p: f(t, p))


def touch(t, flag, args):
    def one(p):
        d, name, ex = t.locate(p)
        if ex is None:
            t.add(d, name, Entry(name, d, False))
        return []
    return each_path("touch", args, one)


def put(t, args, appending):
    p = args[0]
    text = " ".join(args[1:]) + "\n"
    d, name, ex = t.locate(p)
    if ex is not None and ex.is_dir:
        raise FsError("is a directory")
    old = ex.text if ex is not None else ""
    new = old + text if appending else text
    if not t.fits(t.usage() - len(old) + len(new)):
        raise FsError("no space left")
    if ex is None:
        ex = Entry(name, d, False)
        t.add(d, name, ex)
    ex.text = new
    return []


def write(t, flag, args):
    return put(t, args, False)


def append(t, flag, args):
    return put(t, args, True)


def cat(t, flag, args):
    def one(p):
        e = t.resolve(p)
        if e.is_dir:
            raise FsError("is a directory")
        return [e.text] if e.text else []
    return each_path("cat", args, one)


def ls(t, flag, args):
    paths = args or ["."]

    def one(p):
        e = t.resolve(p)
        if not e.is_dir:
            return listing.ls_lines(t, [(e, p)], flag)
        head = [p + ":"] if len(paths) > 1 else []
        return head + listing.ls_lines(t, [(c, c.name) for c in listing.sorted_children(e)], flag)
    return each_path("ls", paths, one)


def rm(t, flag, args):
    def one(p):
        e = t.resolve(p)
        if e.is_dir:
            if not flag:
                raise FsError("is a directory")
            if t.is_within(t.cwd, e):
                raise FsError("in use")
        t.detach(e)
        return []
    return each_path("rm", args, one)


class Stop(Exception):
    pass


def transfer(t, cmd, recursive, src_p, dst_p):
    """mv / cp steps 1-7; errors are raised as (path, message)."""
    def fail(path, msg):
        raise Stop(f"{cmd}: {path}: {msg}")
    try:
        src = t.resolve(src_p)
    except FsError as e:
        fail(src_p, e)
    if cmd == "cp" and src.is_dir and not recursive:
        fail(src_p, "is a directory")
    try:
        d, name, ex = t.locate(dst_p)
    except FsError as e:
        fail(dst_p, e)
    if ex is not None and ex.is_dir:
        d, name = ex, src.name
        ex = d.children.get(name)
    elif ex is not None:
        d, name = ex.parent, ex.name
    if src.is_dir and t.is_within(d, src):
        fail(src_p, "cannot move into itself" if cmd == "mv" else "cannot copy into itself")
    if ex is src:
        if cmd == "mv":
            return
        fail(dst_p, "same file")
    if ex is not None:
        if not src.is_dir and ex.is_dir:
            fail(dst_p, "is a directory")
        if src.is_dir and not ex.is_dir:
            fail(dst_p, "not a directory")
        if src.is_dir:
            if cmd == "cp":
                fail(dst_p, "file exists")
            if ex.children:
                fail(dst_p, "directory not empty")
            if ex is t.cwd:
                fail(dst_p, "in use")
    if cmd == "cp":
        removed = t.size(ex) if ex is not None else 0
        if not t.fits(t.usage() - removed + t.size(src)):
            fail(dst_p, "no space left")
        if ex is not None:
            t.detach(ex)
        t.add(d, name, t.copy(src))
    else:
        if ex is not None:
            t.detach(ex)
        t.detach(src)
        t.add(d, name, src)


def move_or_copy(cmd):
    def run(t, flag, args):
        try:
            transfer(t, cmd, flag, args[0], args[1])
        except Stop as s:
            return [str(s)]
        return []
    return run


def du(t, flag, args):
    p = args[0] if args else "."
    return each_path("du", [p], lambda p: listing.du_lines(t, t.resolve(p)))


def find(t, flag, args):
    pat = args[2] if len(args) == 3 else None
    return each_path("find", [args[0]], lambda p: listing.find_lines(t, t.resolve(p), pat))


def quota(t, flag, args):
    if not args:
        return [f"{t.usage()} {'none' if t.limit is None else t.limit}"]
    n = int(args[0])
    if t.usage() > n:
        return [f"quota: {args[0]}: below usage"]
    t.limit = n
    return []


def find_ok(args):
    return len(args) == 1 or (len(args) == 3 and args[1] == "-name")


def quota_ok(args):
    return not args or (len(args) == 1 and 1 <= len(args[0]) <= 10 and all("0" <= c <= "9" for c in args[0]))


# name: (flag or None, function, argument count check)
COMMANDS = {
    "pwd": (None, pwd, lambda a: len(a) == 0),
    "cd": (None, cd, lambda a: len(a) == 1),
    "mkdir": ("-p", mkdir, lambda a: len(a) >= 1),
    "touch": (None, touch, lambda a: len(a) >= 1),
    "write": (None, write, lambda a: len(a) >= 1),
    "append": (None, append, lambda a: len(a) >= 1),
    "cat": (None, cat, lambda a: len(a) >= 1),
    "ls": ("-l", ls, lambda a: True),
    "rm": ("-r", rm, lambda a: len(a) >= 1),
    "mv": (None, move_or_copy("mv"), lambda a: len(a) == 2),
    "cp": ("-r", move_or_copy("cp"), lambda a: len(a) == 2),
    "du": (None, du, lambda a: len(a) <= 1),
    "find": (None, find, find_ok),
    "quota": (None, quota, quota_ok),
}
