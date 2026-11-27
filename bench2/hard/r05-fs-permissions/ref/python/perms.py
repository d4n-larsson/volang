"""Who may do what."""
import fmt

BITS = "rwxrwx"


def can(sh, node, bit):
    """Whether the current user has permission BIT (`r`, `w` or `x`) on NODE: root always has;
    the owner has what the owner triple gives, everybody else what the other triple gives."""
    if sh.user == "root":
        return True
    i = "rwx".index(bit)
    if node.owner != sh.user:
        i += 3
    return node.mode[i] == bit


def can_list(sh, node):
    """Whether the current user may list the directory NODE and go into it."""
    return can(sh, node, "r") and can(sh, node, "x")


def denied(names):
    return fmt.error(fmt.DENIED, names)


def is_mode(text):
    if len(text) != 6:
        return False
    for i in range(6):
        if text[i] != BITS[i] and text[i] != "-":
            return False
    return True
