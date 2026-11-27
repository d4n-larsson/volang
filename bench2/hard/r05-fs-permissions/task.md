# File system shell: users and permissions

## The existing program

The program is a shell over a file system held in memory (no real files are touched). It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored; the input is ASCII. A command that succeeds prints nothing unless its output is described below.

The tree starts as the empty root directory `/`, which is also the current directory. A file holds a list of lines of text; its size in bytes counts every line plus one byte for its newline. A directory's entries are always listed and visited in ascending byte order of their names.

**Paths.** A path argument is absolute (it starts with `/`) or relative to the current directory. It is first turned into the list of names that leads to it from `/`, on the text alone: empty parts and `.` are skipped and `..` drops the last name (`..` of `/` is `/`); so `/a/f/..` is `/a` even when `/a/f` is a file or does not exist. The *walk* to the path then starts at `/` and goes down one name at a time: going down from a file is the error `not a directory`, a missing name is `no such file or directory`. Walking to the *parent* of a path (for a path that is not `/`) is the walk to its names but the last; a file there is `not a directory`.

Every error is printed as `error: KIND: PATH`, where PATH is the absolute normalized form of the path concerned (`/` for the root, else `/a/b`), never the text as typed; for walks it is the whole path argument, also when the walk fails earlier on it. The kinds are `no such file or directory`, `not a directory`, `is a directory`, `file exists`, `directory not empty`, `directory in use` and `cannot move a directory into itself`. A directory is *in use* when it is the current directory or contains it (`/` always is).

Commands (checks happen in the order given; on an error nothing changes):

- `pwd`: prints the current directory. `cd PATH`: walks to PATH; a file is `not a directory`.
- `mkdir PATH`: `/` is `file exists`; walks to the parent; an existing entry is `file exists`. `mkdir -p PATH` creates every missing directory on the path, going down from `/` name by name; existing directories are fine; a file in the middle of the path is `not a directory`, a file at its end `file exists`.
- `rmdir PATH`: walks; a file is `not a directory`; then `directory in use`; then a directory with entries is `directory not empty`.
- `write PATH TEXT...` and `append PATH TEXT...` (TEXT: the remaining words joined by single spaces, possibly empty): `/` is `is a directory`; walks to the parent; an existing directory is `is a directory`. A missing file is created. `write` replaces the file's lines with the one line TEXT (no line at all when TEXT is empty); `append` adds TEXT as a new last line.
- `touch PATH`: for `/` nothing happens; walks to the parent; creates an empty file when nothing is there, else changes nothing.
- `cat PATH`, `head PATH N` (N a number: the first N lines) and `grep WORD PATH` (each line containing WORD as `NUMBER:LINE`, numbered from 1): walks; a directory is `is a directory`.
- `ls [PATH]` (PATH defaults to `.`): walks; prints a directory's entries one per line, directories with a trailing `/`, or `(empty)`; a file is printed as its own name.
- `rm PATH` removes a file. A directory is `is a directory`, unless the command is `rm -r PATH`, which removes the directory and everything below it, after a `directory in use` check.
- `mv SRC DST`: walks to SRC; a directory in use is `directory in use`. Then the target T: DST is walked; if that fails with an error other than `no such file or directory`, the error is printed (naming DST); if DST is an existing directory, T is DST with the last name of SRC added, else T is DST. A directory SRC with T equal to it or below it is `cannot move a directory into itself` (naming SRC). Then the parent of T is walked (errors name T). An existing entry at T is `file exists` (naming T), unless both SRC and that entry are files: then the file at T is replaced. SRC then moves to T.
- `cp SRC DST`: walks to SRC; a directory is `is a directory`. The target T is found as for `mv` and its parent walked; an existing directory at T is `file exists`; an existing file at T gets a copy of SRC's lines; otherwise a new file is created with them.
- `stat PATH`: walks; prints `PATH: file, B bytes, L lines` or `PATH: directory, N entries`.
- `tree [PATH]` (default `.`): walks; prints PATH, then every entry below it in tree order (a directory before its entries), indented two spaces per level, directories with a trailing `/`.
- `du [PATH]` (default `.`): walks; prints `SIZE PATH` for each directory of the subtree, each one after the directories inside it, SIZE being the total size of all files below; for a file, one line with its own size.
- `find PATH NAME`: walks; prints, in tree order, every path of the subtree (PATH included) whose last name is NAME, or `no matches`.

A command with the wrong number of arguments (or a non-number N for `head`) prints `error: bad command`. A first word that is not a command prints `error: unknown command WORD`.

## The change

Add users, owners and permissions, and make every command respect them.

**Users.** The user `root` exists from the start and is the current user.

- `whoami`: prints the current user.
- `useradd NAME`: `error: only root can add users` when the current user is not root, then `error: user exists: NAME` if there is such a user; otherwise adds the user.
- `su NAME`: `error: no such user: NAME`, or NAME becomes the current user (no password). The current directory stays what it was.

**Owners and modes.** Every file and directory has an owner (a user) and a mode: six characters, two triples, the first for the owner and the second for every other user; in each triple the positions are `r`, `w`, `x`, each either that letter or `-` (for example `rwxr-x`). `/` belongs to root with mode `rwxr-x`. A directory made by `mkdir` or `mkdir -p` belongs to the current user with mode `rwxr-x`. A file created by `write`, `append`, `touch` or `cp` belongs to the current user with mode `rw-r--`. A file whose lines `write`, `append` or `cp` change keeps its owner and mode, and `mv` keeps the owner and mode of what it moves.

**Who may do what.** root has every permission on everything, and none of the checks below ever fails for root. Another user has a permission on a node when its letter is set in the owner triple if the user owns the node, and in the other triple if not (an owner gets only the owner triple, even when the other triple allows more). A failed check prints `error: permission denied: PATH`, with PATH as for the other errors.

1. **Walks**: going down from a directory to one of its entries needs `x` on that directory. In every walk of every command, at each name: first `not a directory` (when the current node is a file), then `permission denied` (no `x` on the current directory), then `no such file or directory`. A walk needs no permission on the node it ends at. Walking to the parent of a path also needs `x` on that parent (checked right after it is found to be a directory), since the command will look at the last name inside it. This covers relative paths too: they are walked from `/`, through the current directory.
2. **Per command**, each check comes after all of the command's existing checks unless stated otherwise:
   - `cd`: `x` on the directory.
   - `mkdir`: `w` on the parent (so an existing entry is still `file exists`). `mkdir -p`: at each name, `x` on the directory it is looked up in (before looking), and `w` on that directory when the name is missing and gets created.
   - `rmdir`, `rm`, `rm -r`: `w` on the parent. Then `rm -r` on a directory needs `r`, `w` and `x` on that directory and on every directory below it; the first one in tree order (entries in byte order) that lacks any of them is reported as `permission denied: Q`, Q being that directory's path, and nothing is removed. Files need no permission to be removed.
   - `write`, `append`: `w` on the file when it exists, else `w` on the parent. `touch`: `w` on the parent when the file is created; nothing when something is already there.
   - `cat`, `head`, `grep`: `r` on the file.
   - `cp`: `r` on SRC, checked right after SRC's `is a directory` check (before the target is looked at). At the end: `w` on the existing file at T, or `w` on T's parent when the file is new (errors name T).
   - `mv`: at the end, `w` on SRC's parent (`permission denied: SRC`), then `w` on T's parent (`permission denied: T`). Replacing a file at T needs no permission on that file.
   - `ls` on a directory: `r` on it. A file needs nothing.
   - `tree`, `du`, `find` on a directory PATH: `r` and `x` on it, else `permission denied: PATH` is the only output. Below PATH, a directory the user lacks `r` or `x` on is not entered: `tree` prints its line with ` [denied]` added (for example `  private/ [denied]`) and nothing below it; `du` prints `denied PATH` in its place (PATH that directory's path) and counts it as 0 bytes in the directories above; `find` can still match that directory's own name but does not search inside it. A file PATH needs nothing.
   - `stat` needs nothing more; its line ends with `, owner USER, mode MODE`, for example `/a/f: file, 9 bytes, 1 lines, owner ann, mode rw-r--`.
3. **`ls -l [PATH]`**: `-l` is an optional first argument of `ls` (only the first argument can be the flag: in `ls -l -l` the second `-l` is the path). On a directory it needs `r` and `x`, and prints one line per entry, `TMODE OWNER SIZE NAME`, where T is `d` for a directory and `-` for a file, SIZE is a file's size in bytes or a directory's number of entries, and NAME has a trailing `/` for a directory; an empty directory prints `(empty)`. On a file it prints that line for the file itself and needs nothing. For example `drwxr-x ann 2 work/` and `-rw-r-- ann 18 todo.txt`.
4. **`chmod MODE PATH`**: a MODE that is not six characters of the form above is `error: bad mode MODE` (checked first); walks to PATH; a user who is neither root nor the owner gets `permission denied`; otherwise the node gets the mode.
5. **`chown USER PATH`**: walks to PATH; a user other than root gets `permission denied`; an unknown USER is `error: no such user: USER`; otherwise USER becomes the owner.

`whoami` takes no argument, `useradd` and `su` one, `chmod` and `chown` two; another number is `error: bad command`.

Everything else stays exactly as it is.
