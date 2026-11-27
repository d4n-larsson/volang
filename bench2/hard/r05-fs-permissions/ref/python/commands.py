"""The command dispatcher: checks the number and form of the arguments, then runs the command."""
import dircmds
import filecmds
import usercmds
import walkcmds

BAD = ["error: bad command"]


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def optional_path(args):
    """The one optional path argument of ls, tree and du: `.` when it is left out."""
    if len(args) == 0:
        return "."
    return args[0]


def flagged(args, flag):
    """Splits an optional leading FLAG (`-p`, `-r`) off the arguments."""
    if len(args) > 0 and args[0] == flag:
        return True, args[1:]
    return False, args


def run_dir_command(sh, cmd, args):
    if cmd == "pwd":
        return BAD if args else dircmds.pwd(sh)
    if cmd == "cd":
        return BAD if len(args) != 1 else dircmds.cd(sh, args[0])
    if cmd == "mkdir":
        parents, rest = flagged(args, "-p")
        if len(rest) != 1:
            return BAD
        if parents:
            return dircmds.mkdir_parents(sh, rest[0])
        return dircmds.mkdir(sh, rest[0])
    if cmd == "rmdir":
        return BAD if len(args) != 1 else dircmds.rmdir(sh, args[0])
    if cmd == "ls":
        long, rest = flagged(args, "-l")
        return BAD if len(rest) > 1 else dircmds.ls(sh, optional_path(rest), long)
    if cmd == "rm":
        recursive, rest = flagged(args, "-r")
        if len(rest) != 1:
            return BAD
        return dircmds.rm(sh, rest[0], recursive)
    if cmd == "mv":
        return BAD if len(args) != 2 else dircmds.mv(sh, args[0], args[1])
    return None


def run_file_command(sh, cmd, args):
    if cmd == "write" or cmd == "append":
        if len(args) < 1:
            return BAD
        return filecmds.write_text(sh, args[0], args[1:], cmd == "append")
    if cmd == "touch":
        return BAD if len(args) != 1 else filecmds.touch(sh, args[0])
    if cmd == "cat":
        return BAD if len(args) != 1 else filecmds.cat(sh, args[0])
    if cmd == "head":
        if len(args) != 2 or not is_number(args[1]):
            return BAD
        return filecmds.head(sh, args[0], int(args[1]))
    if cmd == "grep":
        return BAD if len(args) != 2 else filecmds.grep(sh, args[0], args[1])
    if cmd == "cp":
        return BAD if len(args) != 2 else filecmds.cp(sh, args[0], args[1])
    if cmd == "stat":
        return BAD if len(args) != 1 else filecmds.stat(sh, args[0])
    return None


def run_walk_command(sh, cmd, args):
    if cmd == "tree":
        return BAD if len(args) > 1 else walkcmds.tree(sh, optional_path(args))
    if cmd == "du":
        return BAD if len(args) > 1 else walkcmds.du(sh, optional_path(args))
    if cmd == "find":
        return BAD if len(args) != 2 else walkcmds.find(sh, args[0], args[1])
    return None


def run_user_command(sh, cmd, args):
    if cmd == "whoami":
        return BAD if args else usercmds.whoami(sh)
    if cmd == "useradd":
        return BAD if len(args) != 1 else usercmds.useradd(sh, args[0])
    if cmd == "su":
        return BAD if len(args) != 1 else usercmds.su(sh, args[0])
    if cmd == "chmod":
        return BAD if len(args) != 2 else usercmds.chmod(sh, args[0], args[1])
    if cmd == "chown":
        return BAD if len(args) != 2 else usercmds.chown(sh, args[0], args[1])
    return None


def run(sh, words):
    cmd = words[0]
    args = words[1:]
    for group in (run_dir_command, run_file_command, run_walk_command, run_user_command):
        out = group(sh, cmd, args)
        if out is not None:
            return out
    return ["error: unknown command " + cmd]
