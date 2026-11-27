"""Commands that go through a whole subtree: tree, du, find."""
import fmt
import nodes
import paths
import perms
import resolve


def tree(sh, text):
    """The path, then every entry below it, indented two spaces per level, in byte order."""
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if node.is_dir and not perms.can_list(sh, node):
        return [perms.denied(names)]
    out = [paths.display(names)]
    if node.is_dir:
        tree_lines(sh, node, 1, out)
    return out


def tree_lines(sh, node, depth, out):
    for name in nodes.sorted_names(node):
        item = nodes.entry(node, name)
        line = fmt.indent(depth) + fmt.entry_name(name, item)
        if item.is_dir and not perms.can_list(sh, item):
            out.append(line + " [denied]")
            continue
        out.append(line)
        if item.is_dir:
            tree_lines(sh, item, depth + 1, out)


def du(sh, text):
    """`SIZE PATH` for every directory of the subtree, each after the directories inside it;
    SIZE is the total size of the files below. A file gets one line of its own size."""
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if not node.is_dir:
        return [fmt.size_line(nodes.file_size(node), names)]
    if not perms.can_list(sh, node):
        return [perms.denied(names)]
    out = []
    usage(sh, node, names, out)
    return out


def usage(sh, node, names, out):
    total = 0
    for name in nodes.sorted_names(node):
        item = nodes.entry(node, name)
        if item.is_dir and not perms.can_list(sh, item):
            out.append("denied " + paths.display(paths.child(names, name)))
        elif item.is_dir:
            total += usage(sh, item, paths.child(names, name), out)
        else:
            total += nodes.file_size(item)
    out.append(fmt.size_line(total, names))
    return total


def find(sh, text, wanted):
    """The paths in the subtree (the start included) whose last name is WANTED, in tree order."""
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if node.is_dir and not perms.can_list(sh, node):
        return [perms.denied(names)]
    out = []
    search(sh, node, names, wanted, out)
    if not out:
        return ["no matches"]
    return out


def search(sh, node, names, wanted, out):
    """A directory the user cannot list can match itself, but its entries are not searched."""
    if names and paths.base_name(names) == wanted:
        out.append(paths.display(names))
    if node.is_dir and perms.can_list(sh, node):
        for name in nodes.sorted_names(node):
            search(sh, nodes.entry(node, name), paths.child(names, name), wanted, out)
