"""Output formatting: listings, du, find and name patterns."""


def sorted_children(d):
    return [d.children[n] for n in sorted(d.children, key=lambda n: n.encode())]


def ls_lines(tree, entries_named, long):
    """entries_named: [(entry, shown name)]."""
    rows = []
    for e, name in entries_named:
        rows.append((e, name + ("/" if e.is_dir else "")))
    if not long:
        return [n for _, n in rows]
    sizes = [str(tree.size(e)) for e, _ in rows]
    w = max((len(s) for s in sizes), default=0)
    return [("d" if e.is_dir else "-") + " " + s.rjust(w) + " " + n for (e, n), s in zip(rows, sizes)]


def tree_order(e):
    yield e
    if e.is_dir:
        for c in sorted_children(e):
            yield from tree_order(c)


def du_lines(tree, e):
    return [f"{tree.size(x)} {tree.abspath(x)}" for x in tree_order(e) if x is e or x.is_dir]


def matches(pat, name):
    # dp over pattern and name
    memo = {}

    def m(i, j):
        if (i, j) in memo:
            return memo[(i, j)]
        if i == len(pat):
            r = j == len(name)
        elif pat[i] == "*":
            r = m(i + 1, j) or (j < len(name) and m(i, j + 1))
        elif j < len(name) and (pat[i] == "?" or pat[i] == name[j]):
            r = m(i + 1, j + 1)
        else:
            r = False
        memo[(i, j)] = r
        return r
    return m(0, 0)


def find_lines(tree, e, pattern):
    return [tree.abspath(x) for x in tree_order(e) if pattern is None or matches(pattern, x.name)]
