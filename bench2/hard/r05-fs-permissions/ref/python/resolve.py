"""Finding the node a path names."""
import fmt
import nodes
import perms


def walk_to(sh, names, shown):
    """The node at the path NAMES, or an error naming the path SHOWN.

    The walk starts at / and goes down one name at a time: going down from a file is
    `not a directory`, a missing name is `no such file or directory`."""
    node = sh.root
    for name in names:
        if not node.is_dir:
            return None, fmt.error(fmt.NOT_DIR, shown)
        if not perms.can(sh, node, "x"):
            return None, perms.denied(shown)
        if not nodes.has_entry(node, name):
            return None, fmt.error(fmt.NO_ENTRY, shown)
        node = nodes.entry(node, name)
    return node, ""


def walk(sh, names):
    """The node at the path NAMES, or an error naming that path."""
    return walk_to(sh, names, names)


def walk_parent(sh, names):
    """The directory that holds (or would hold) the last name of NAMES, which is not the root.

    Errors name the whole path NAMES."""
    holder, err = walk_to(sh, names[:-1], names)
    if err:
        return None, err
    if not holder.is_dir:
        return None, fmt.error(fmt.NOT_DIR, names)
    if not perms.can(sh, holder, "x"):
        return None, perms.denied(names)
    return holder, ""


def target(sh, src_names, dst_names):
    """Where `mv` and `cp` put SRC: inside DST if DST is an existing directory, else at DST.

    Errors other than a missing DST are returned (naming DST)."""
    dst, err = walk(sh, dst_names)
    if err and not fmt.is_no_entry(err):
        return None, err
    if not err and dst.is_dir:
        return dst_names + [src_names[-1]], ""
    return dst_names, ""
