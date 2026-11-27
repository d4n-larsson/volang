"""Commands on files: write, append, touch, cat, head, grep, cp, stat."""
import fmt
import nodes
import paths
import perms
import resolve


def write_text(sh, text, words, append):
    """`write PATH TEXT...` replaces the file's lines with the one line TEXT (no line when TEXT is
    empty); `append PATH TEXT...` adds TEXT as a new last line. Both create a missing file."""
    names = paths.normalize(sh.cwd, text)
    if not names:
        return [fmt.error(fmt.IS_DIR, names)]
    holder, err = resolve.walk_parent(sh, names)
    if err:
        return [err]
    name = paths.base_name(names)
    line = fmt.text_of(words)
    if nodes.has_entry(holder, name):
        node = nodes.entry(holder, name)
        if node.is_dir:
            return [fmt.error(fmt.IS_DIR, names)]
        if not perms.can(sh, node, "w"):
            return [perms.denied(names)]
    else:
        if not perms.can(sh, holder, "w"):
            return [perms.denied(names)]
        node = nodes.new_file([], sh.user)
        nodes.put_entry(holder, name, node)
    if append:
        node.lines.append(line)
    elif line == "":
        node.lines = []
    else:
        node.lines = [line]
    return []


def touch(sh, text):
    """Creates an empty file if nothing is at the path; an existing file or directory is left alone."""
    names = paths.normalize(sh.cwd, text)
    if not names:
        return []
    holder, err = resolve.walk_parent(sh, names)
    if err:
        return [err]
    name = paths.base_name(names)
    if not nodes.has_entry(holder, name):
        if not perms.can(sh, holder, "w"):
            return [perms.denied(names)]
        nodes.put_entry(holder, name, nodes.new_file([], sh.user))
    return []


def readable_file(sh, text):
    """The file at the path TEXT, for the commands that read one."""
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return None, err
    if node.is_dir:
        return None, fmt.error(fmt.IS_DIR, names)
    if not perms.can(sh, node, "r"):
        return None, perms.denied(names)
    return node, ""


def cat(sh, text):
    node, err = readable_file(sh, text)
    if err:
        return [err]
    return list(node.lines)


def head(sh, text, count):
    node, err = readable_file(sh, text)
    if err:
        return [err]
    return node.lines[:count]


def grep(sh, word, text):
    """The lines of the file that contain WORD, each as `NUMBER:LINE` (lines numbered from 1)."""
    node, err = readable_file(sh, text)
    if err:
        return [err]
    out = []
    for i, line in enumerate(node.lines):
        if word in line:
            out.append(fmt.numbered(i + 1, line))
    return out


def cp(sh, src_text, dst_text):
    """Copies a file. An existing file at the target gets the source's lines."""
    src_names = paths.normalize(sh.cwd, src_text)
    src, err = resolve.walk(sh, src_names)
    if err:
        return [err]
    if src.is_dir:
        return [fmt.error(fmt.IS_DIR, src_names)]
    if not perms.can(sh, src, "r"):
        return [perms.denied(src_names)]
    target, err = resolve.target(sh, src_names, paths.normalize(sh.cwd, dst_text))
    if err:
        return [err]
    holder, err = resolve.walk_parent(sh, target)
    if err:
        return [err]
    name = paths.base_name(target)
    if nodes.has_entry(holder, name):
        existing = nodes.entry(holder, name)
        if existing.is_dir:
            return [fmt.error(fmt.EXISTS, target)]
        if not perms.can(sh, existing, "w"):
            return [perms.denied(target)]
        existing.lines = list(src.lines)
        return []
    if not perms.can(sh, holder, "w"):
        return [perms.denied(target)]
    nodes.put_entry(holder, name, nodes.new_file(src.lines, sh.user))
    return []


def stat(sh, text):
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    return [fmt.stat_line(names, node)]
