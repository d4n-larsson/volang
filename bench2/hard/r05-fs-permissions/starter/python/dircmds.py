"""Commands on directories: mkdir, rmdir, cd, pwd, ls, rm, mv."""
import fmt
import nodes
import paths
import resolve


def mkdir(sh, text):
    names = paths.normalize(sh.cwd, text)
    if not names:
        return [fmt.error(fmt.EXISTS, names)]
    holder, err = resolve.walk_parent(sh, names)
    if err:
        return [err]
    name = paths.base_name(names)
    if nodes.has_entry(holder, name):
        return [fmt.error(fmt.EXISTS, names)]
    nodes.put_entry(holder, name, nodes.new_dir())
    return []


def mkdir_parents(sh, text):
    """`mkdir -p`: creates every missing directory on the path; existing directories are fine."""
    names = paths.normalize(sh.cwd, text)
    node = sh.root
    for i, name in enumerate(names):
        if nodes.has_entry(node, name):
            node = nodes.entry(node, name)
            if not node.is_dir:
                if i == len(names) - 1:
                    return [fmt.error(fmt.EXISTS, names)]
                return [fmt.error(fmt.NOT_DIR, names)]
        else:
            made = nodes.new_dir()
            nodes.put_entry(node, name, made)
            node = made
    return []


def in_use(sh, names):
    """Whether the directory NAMES is the current directory or holds it (/ always does)."""
    return paths.is_within(sh.cwd, names)


def rmdir(sh, text):
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if not node.is_dir:
        return [fmt.error(fmt.NOT_DIR, names)]
    if in_use(sh, names):
        return [fmt.error(fmt.IN_USE, names)]
    if nodes.entry_count(node) > 0:
        return [fmt.error(fmt.NOT_EMPTY, names)]
    holder, _ = resolve.walk_parent(sh, names)
    nodes.drop_entry(holder, paths.base_name(names))
    return []


def cd(sh, text):
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if not node.is_dir:
        return [fmt.error(fmt.NOT_DIR, names)]
    sh.cwd = names
    return []


def pwd(sh):
    return [paths.display(sh.cwd)]


def ls(sh, text):
    """The entries of a directory, one per line; a file is listed as its own name."""
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if not node.is_dir:
        return [paths.base_name(names)]
    out = []
    for name in nodes.sorted_names(node):
        out.append(fmt.entry_name(name, nodes.entry(node, name)))
    if not out:
        return ["(empty)"]
    return out


def rm(sh, text, recursive):
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if node.is_dir:
        if not recursive:
            return [fmt.error(fmt.IS_DIR, names)]
        if in_use(sh, names):
            return [fmt.error(fmt.IN_USE, names)]
    holder, _ = resolve.walk_parent(sh, names)
    nodes.drop_entry(holder, paths.base_name(names))
    return []


def mv(sh, src_text, dst_text):
    """Moves (renames) a file or directory. A file may replace an existing file."""
    src_names = paths.normalize(sh.cwd, src_text)
    src, err = resolve.walk(sh, src_names)
    if err:
        return [err]
    if src.is_dir and in_use(sh, src_names):
        return [fmt.error(fmt.IN_USE, src_names)]
    target, err = resolve.target(sh, src_names, paths.normalize(sh.cwd, dst_text))
    if err:
        return [err]
    if src.is_dir and paths.is_within(target, src_names):
        return [fmt.error(fmt.INTO_ITSELF, src_names)]
    holder, err = resolve.walk_parent(sh, target)
    if err:
        return [err]
    name = paths.base_name(target)
    if nodes.has_entry(holder, name):
        if src.is_dir or nodes.entry(holder, name).is_dir:
            return [fmt.error(fmt.EXISTS, target)]
    src_holder, _ = resolve.walk_parent(sh, src_names)
    nodes.drop_entry(src_holder, paths.base_name(src_names))
    nodes.put_entry(holder, name, src)
    return []
