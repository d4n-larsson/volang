"""Checking and running statements against the in-memory tables."""
from syntax import Agg, Col, Lit


class SemErr(Exception):
    pass


class Table:
    def __init__(self, name, cols):
        self.name = name
        self.cols = cols                   # [(name, type)]
        self.rows = []


def lit_type(v):
    if v is None:
        return None
    return "int" if isinstance(v, int) else "text"


# ---- checking ----

class Scope:
    def __init__(self, tables, grouped=False):
        self.tables = tables
        self.source = [(t.name, c, ty) for t in tables for c, ty in t.cols]
        self.grouped = grouped
        self.group_idx = set()

    def find(self, col):
        if col.table:
            hits = [i for i, (t, c, _) in enumerate(self.source) if t == col.table and c == col.name]
        else:
            hits = [i for i, (_, c, _) in enumerate(self.source) if c == col.name]
            if len(hits) > 1:
                raise SemErr(f"ambiguous column {col.name}")
        if not hits:
            raise SemErr(f"unknown column {col.text()}")
        return hits[0]

    def resolve(self, col):
        col.index = self.find(col)
        return self.source[col.index][2]


def check_operand(sc, o, part):
    if isinstance(o, Lit):
        return lit_type(o.value)
    if isinstance(o, Col):
        ty = sc.resolve(o)
        if sc.grouped and part in ("select", "having", "order") and o.index not in sc.group_idx:
            raise SemErr(f"column {o.text()} must be grouped")
        return ty
    if part == "where":
        raise SemErr("aggregate in where")
    if o.arg is None:
        return "int"
    ty = sc.resolve(o.arg)
    if o.func == "sum" and ty != "int":
        raise SemErr("sum needs an int column")
    return "int" if o.func in ("count", "sum") else ty


def check_expr(sc, e, part):
    if e.kind in ("and", "or"):
        check_expr(sc, e.a, part)
        check_expr(sc, e.b, part)
    elif e.kind == "not":
        check_expr(sc, e.a, part)
    elif e.kind in ("isnull", "notnull"):
        check_operand(sc, e.a, part)
    else:
        ta = check_operand(sc, e.a, part)
        tb = check_operand(sc, e.b, part)
        if ta and tb and ta != tb:
            raise SemErr("type mismatch in comparison")


def has_agg(e):
    if e is None:
        return False
    if isinstance(e, Agg):
        return True
    if isinstance(e, (Col, Lit)):
        return False
    return has_agg(e.a) or has_agg(e.b)


def table(db, name):
    if name not in db:
        raise SemErr(f"unknown table {name}")
    return db[name]


def check_select(db, s):
    tables = [table(db, s.table)]
    if s.join:
        tables.append(table(db, s.join))
        if s.join == s.table:
            raise SemErr(f"duplicate table {s.join}")
    grouped = (s.group is not None or any(isinstance(i, Agg) for i in s.items or [])
               or has_agg(s.having) or any(isinstance(k, Agg) for k, _ in s.order))
    sc = Scope(tables, grouped)
    for c in s.group or []:
        try:
            sc.group_idx.add(sc.find(c))
        except SemErr:
            pass
    if s.items is None:
        if grouped:
            raise SemErr("* with grouping")
    else:
        for it in s.items:
            check_operand(sc, it, "select")
    if s.on:
        check_expr(sc, s.on, "on")
    if s.where:
        check_expr(sc, s.where, "where")
    for c in s.group or []:
        sc.resolve(c)
    if s.having:
        if not grouped:
            raise SemErr("having without grouping")
        check_expr(sc, s.having, "having")
    for k, _ in s.order:
        check_operand(sc, k, "order")
    return sc


# ---- evaluation ----

def compare(a, b):
    """-1, 0 or 1; NULL (None) is smallest."""
    if a is None or b is None:
        return (a is not None) - (b is not None)
    return (a > b) - (a < b)


def aggregate(agg, rows):
    if agg.arg is None:
        return len(rows)
    vals = [r[agg.arg.index] for r in rows if r[agg.arg.index] is not None]
    if agg.func == "count":
        return len(vals)
    if not vals:
        return None
    return {"sum": sum, "min": min, "max": max}[agg.func](vals)


class Group:
    def __init__(self, rows):
        self.rows = rows


def value(o, row):
    """row is a combined row (list), or a Group."""
    if isinstance(o, Lit):
        return o.value
    if isinstance(o, Agg):
        return aggregate(o, row.rows)
    if isinstance(row, Group):
        return row.rows[0][o.index]
    return row[o.index]


def truth(e, row):
    if e.kind == "and":
        a, b = truth(e.a, row), truth(e.b, row)
        return False if a is False or b is False else (None if a is None or b is None else True)
    if e.kind == "or":
        a, b = truth(e.a, row), truth(e.b, row)
        return True if a is True or b is True else (None if a is None or b is None else False)
    if e.kind == "not":
        a = truth(e.a, row)
        return None if a is None else not a
    if e.kind == "isnull":
        return value(e.a, row) is None
    if e.kind == "notnull":
        return value(e.a, row) is not None
    a, b = value(e.a, row), value(e.b, row)
    if a is None or b is None:
        return None
    c = compare(a, b)
    return {"=": c == 0, "!=": c != 0, "<": c < 0, "<=": c <= 0, ">": c > 0, ">=": c >= 0}[e.op]


def source_rows(db, s):
    left = db[s.table].rows
    if not s.join:
        return [list(r) for r in left]
    right = db[s.join].rows
    width = len(db[s.join].cols)
    out = []
    for lr in left:
        kept = [lr + rr for rr in right if truth(s.on, lr + rr) is True]
        if not kept and s.left:
            kept = [lr + [None] * width]
        out += kept
    return out


def sort_stable(rows, keys):
    import functools

    def cmp(x, y):
        for k, desc in keys:
            c = compare(value(k, x), value(k, y))
            if c:
                return -c if desc else c
        return 0
    return sorted(rows, key=functools.cmp_to_key(cmp))


def run_select(db, s):
    """(headers, types, rows of values)."""
    sc = check_select(db, s)
    rows = [r for r in source_rows(db, s) if s.where is None or truth(s.where, r) is True]
    if sc.grouped:
        if s.group is None:
            rows = [Group(rows)]
        else:
            groups = {}
            for r in rows:
                groups.setdefault(tuple(r[c.index] for c in s.group), []).append(r)
            rows = [Group(g) for g in groups.values()]
        if s.having:
            rows = [g for g in rows if truth(s.having, g) is True]
    rows = sort_stable(rows, s.order)
    rows = rows[s.offset:]
    if s.limit is not None:
        rows = rows[:s.limit]
    if s.items is None:
        items = []
        for i, (_, c, _) in enumerate(sc.source):
            col = Col("", c)
            col.index = i
            items.append(col)
        headers = [c for _, c, _ in sc.source]
    else:
        items = s.items
        headers = [it.text() for it in items]
    types = [item_type(sc, it) for it in items]
    return headers, types, [[value(it, r) for it in items] for r in rows]


def item_type(sc, it):
    if isinstance(it, Col):
        return sc.source[it.index][2]
    if it.func in ("count", "sum"):
        return "int"
    return sc.source[it.arg.index][2]


def run_create(db, s):
    if s.table in db:
        raise SemErr(f"table {s.table} already exists")
    seen = set()
    for c, _ in s.cols:
        if c in seen:
            raise SemErr(f"duplicate column {c}")
        seen.add(c)
    db[s.table] = Table(s.table, s.cols)
    return f"created {s.table}"


def fits(ty, lit):
    t = lit_type(lit.value)
    return t is None or t == ty


def run_insert(db, s):
    t = table(db, s.table)
    for tup in s.tuples:
        if len(tup) != len(t.cols):
            raise SemErr("wrong number of values")
        for (c, ty), lit in zip(t.cols, tup):
            if not fits(ty, lit):
                raise SemErr(f"type mismatch for column {c}")
    for tup in s.tuples:
        t.rows.append([lit.value for lit in tup])
    return f"inserted {len(s.tuples)}"


def matching(t, s):
    sc = Scope([t])
    if s.where:
        check_expr(sc, s.where, "where")
    return [s.where is None or truth(s.where, r) is True for r in t.rows]


def run_delete(db, s):
    t = table(db, s.table)
    hits = matching(t, s)
    t.rows = [r for r, h in zip(t.rows, hits) if not h]
    return f"deleted {sum(hits)}"


def run_update(db, s):
    t = table(db, s.table)
    names = [c for c, _ in t.cols]
    done = []
    for c, lit in s.sets:
        if c not in names:
            raise SemErr(f"unknown column {c}")
        if c in done:
            raise SemErr(f"duplicate column {c}")
        done.append(c)
        if not fits(t.cols[names.index(c)][1], lit):
            raise SemErr(f"type mismatch for column {c}")
    hits = matching(t, s)
    for r, h in zip(t.rows, hits):
        if h:
            for c, lit in s.sets:
                r[names.index(c)] = lit.value
    return f"updated {sum(hits)}"
