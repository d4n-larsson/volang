"""Directives: the settings they change, list state, and their errors."""

NAMES = (".width", ".page", ".indent", ".align", ".title", ".heading", ".list", ".endlist", ".break",
         ".space", ".contents")
ALIGNS = ("left", "right", "center", "justify")


class List:
    def __init__(self, kind, depth):
        self.kind = kind
        self.depth = depth
        self.count = 0


class State:
    def __init__(self):
        self.width = 40
        self.height = 20
        self.indent = 0
        self.align = "left"
        self.title = ""
        self.lists = []
        self.started = False      # a text line or a valid heading has been seen
        self.brk = False          # a valid .break since the previous block
        self.space = None         # total of the .space directives since the previous block
        self.contents = False


def number(s):
    if 1 <= len(s) <= 3 and s.isdigit() and s.isascii():
        return int(s)
    return None


def apply(st, fields):
    """Applies a directive. Returns (error message or None, heading text or None)."""
    name, args = fields[0], fields[1:]
    if name not in NAMES:
        return "unknown directive " + name, None
    if name in (".width", ".page", ".indent"):
        n = number(args[0]) if len(args) == 1 else None
        if n is None:
            return "bad argument", None
        if name == ".width":
            if not (20 <= n <= 100 and n - st.indent >= 20):
                return "bad argument", None
        elif name == ".page":
            if not 4 <= n <= 200:
                return "bad argument", None
        elif st.width - n < 20:
            return "bad argument", None
        if name != ".indent" and st.started:
            return "too late", None
        if name == ".width":
            st.width = n
        elif name == ".page":
            st.height = n
        else:
            st.indent = n
        return None, None
    if name == ".space":
        n = number(args[0]) if len(args) == 1 else None
        if n is None or not 1 <= n <= 9:
            return "bad argument", None
        st.space = (st.space or 0) + n
        return None, None
    if name == ".align":
        if len(args) != 1 or args[0] not in ALIGNS:
            return "bad argument", None
        st.align = args[0]
        return None, None
    if name == ".title":
        st.title = " ".join(args)
        return None, None
    if name == ".heading":
        if not args:
            return "bad argument", None
        st.started = True
        text = "".join(c.upper() if "a" <= c <= "z" else c for c in " ".join(args))
        return None, text
    if name == ".list":
        if len(args) != 1 or args[0] not in ("bullet", "number"):
            return "bad argument", None
        if len(st.lists) == 3:
            return "too deep", None
        st.lists.append(List(args[0], len(st.lists) + 1))
        return None, None
    if args:
        return "bad argument", None
    if name == ".endlist":
        if not st.lists:
            return "no list", None
        st.lists.pop()
        return None, None
    if name == ".contents":
        st.contents = True
        return None, None
    st.brk = True
    return None, None
