"""Users and the owners and modes of nodes: whoami, useradd, su, chmod, chown."""
import paths
import perms
import resolve


def whoami(sh):
    return [sh.user]


def useradd(sh, name):
    if sh.user != "root":
        return ["error: only root can add users"]
    if name in sh.users:
        return ["error: user exists: " + name]
    sh.users[name] = True
    return []


def su(sh, name):
    """Switches to another user; the current directory stays."""
    if name not in sh.users:
        return ["error: no such user: " + name]
    sh.user = name
    return []


def chmod(sh, mode, text):
    if not perms.is_mode(mode):
        return ["error: bad mode " + mode]
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if sh.user != "root" and sh.user != node.owner:
        return [perms.denied(names)]
    node.mode = mode
    return []


def chown(sh, user, text):
    names = paths.normalize(sh.cwd, text)
    node, err = resolve.walk(sh, names)
    if err:
        return [err]
    if sh.user != "root":
        return [perms.denied(names)]
    if user not in sh.users:
        return ["error: no such user: " + user]
    node.owner = user
    return []
