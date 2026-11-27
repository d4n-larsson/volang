# Shell file system

Simulate an in-memory file system driven by shell-like commands read from standard input, one command per line. Print what each command prints, in order.

## Program structure

Split the program into at least three source files: the file system tree (entries, path resolution, sizes and absolute paths), the commands (one function per command, with their rules and error messages), and the output formatting (listings, `du`, `find` and pattern matching) each in their own file, plus the entry file that reads standard input, splits lines and dispatches to the commands.

## The tree

The file system is a tree of *entries*. An entry is a *directory*, which holds child entries with distinct names, or a *file*, which holds text. At the start there is only the *root* directory, which is empty and has no name, and the *current directory* is the root. A file's *size* is the number of bytes of its text. A directory's size is the sum of the sizes of all files anywhere below it (0 if there are none).

The *absolute path* of the root is `/`; of any other entry it is `/` followed by the names of the directories from the root down to it and its own name, separated by `/` (for example `/docs/a.txt`).

Names and texts are ASCII. "Byte order" means comparing names byte by byte (ASCII order; a name that is a prefix of another comes first).

## Input

A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored and print nothing. The first field is the command word, the remaining fields are its arguments.

Some commands take a flag. The flag is recognized only as the first argument and only when it is exactly the flag text (`-p`, `-r` or `-l`, as listed for the command); in any other position, or for a command that does not take that flag, a field starting with `-` is an ordinary argument.

A command word that is not one of those below prints `<word>: unknown command`. A command with a wrong number of arguments (after removing its flag) prints `<command>: usage` and does nothing else.

## Paths

A path argument is split at `/` into *components*; empty components are dropped, so repeated slashes and a trailing slash change nothing. A path starting with `/` is absolute and starts at the root; any other path starts at the current directory. A path whose components are all dropped (such as `/` or `//`) names its starting directory.

To *resolve* a path, start at its starting directory and take its components in turn:

- Before taking any component, the entry reached so far must be a directory; otherwise resolution fails with `not a directory`.
- `.` stays where it is. `..` moves to the parent directory; the parent of the root is the root.
- Any other component moves to the child of that name; if there is none, resolution fails with `no such file or directory`.

To *locate* a path (used by the commands that may create or replace an entry): if its last component is `.` or `..`, or it has no components, the path is resolved as a whole and names that existing directory. Otherwise the path without its last component is resolved (its errors are the path's errors); if that is a file, it fails with `not a directory`; then the last component, looked up in that directory, names either an existing child or a *missing name* in that directory.

Every error message has the form `<command>: <path>: <message>`, where `<path>` is the argument exactly as written, unless a different form is given below. A command that takes several paths handles them left to right; an error on one path is printed and the command goes on with the next path.

## Quota

The *usage* is the total size of all files. There is a *limit*, initially none. An operation that would make the usage greater than the limit fails with `no space left` and changes nothing.

## Commands

**`pwd`** (no arguments) prints the absolute path of the current directory.

**`cd PATH`** makes the resolved directory the current directory. Resolution errors are reported; a file is `not a directory`.

**`mkdir [-p] PATH...`** (one or more paths) creates directories.
- Without `-p`: the path is located (errors reported). An existing entry is the error `file exists`; a missing name is created as a new empty directory.
- With `-p`: the components are taken in turn as in resolution, except that a missing name is created as an empty directory and then moved into. Reaching an existing directory is fine. If the last component names an existing file, the error is `file exists`; a file before the last component gives `not a directory` as in resolution. Directories created before an error stay.

**`touch PATH...`** (one or more paths): the path is located (errors reported). An existing entry (file or directory) is left unchanged; a missing name is created as an empty file.

**`write PATH [TEXT...]`** and **`append PATH [TEXT...]`**: the text is the fields after the path joined by single spaces (empty if there are none) followed by a newline character. The path is located (errors reported); an existing directory is the error `is a directory`. `write` sets the file's text to the text, creating the file if it does not exist; `append` adds the text to the end of the existing file's text, or creates the file with the text. The quota is checked on the new usage (the old text of a replaced file no longer counts).

**`cat PATH...`** (one or more paths) prints each file's text exactly (it adds nothing; an empty file prints nothing). A directory is the error `is a directory`.

**`ls [-l] [PATH...]`** (zero or more paths; none means `.`). For each path in turn:
- A resolution error is printed.
- A file prints one line for itself, using the path as written as its name.
- A directory prints one line per child, in byte order of the child names. If more than one path was given, the lines are preceded by a header line `<path>:` (the path as written). An empty directory prints only its header, if any.

A line is the name, with `/` added after a directory's name. With `-l` a line is `<kind> <size> <name>`, where kind is `d` for a directory and `-` for a file, and the size is right-aligned (padded on the left with spaces) to the width of the longest size among the lines printed for that one path argument.

**`rm [-r] PATH...`** (one or more paths) removes entries. A file is removed. A directory without `-r` is the error `is a directory`. With `-r`, a directory is removed with everything below it, unless it is the current directory or contains it at any depth (this includes the root), which is the error `in use`.

**`mv SRC DST`** and **`cp [-r] SRC DST`** (exactly two paths) move or copy one entry. The steps run in this order; the first that fails prints its error and the command stops with nothing changed:

1. Resolve `SRC` (errors reported with `SRC`). For `cp` without `-r`, a directory source is `cp: SRC: is a directory`.
2. Find the *target*, a name in a directory. Locate `DST` (errors reported with `DST`). If it is an existing directory, the target is the source's own name inside that directory (which may or may not exist). If it is an existing file, the target is that file. If it is a missing name, that is the target.
3. If the source is a directory and the target's directory is the source or lies anywhere below it, the error is `mv: SRC: cannot move into itself` or `cp: SRC: cannot copy into itself`.
4. If the target is an existing entry that is the source itself: `mv` does nothing (no error); `cp` prints `cp: DST: same file`.
5. If the target is an existing entry (errors in this step are reported with `DST`): a file source replaces a file target. A file source onto a directory target is the error `is a directory`; a directory source onto a file target is `not a directory`. For a directory source onto a directory target, `cp` always fails with `file exists`; `mv` fails with `directory not empty` if the target is not empty, otherwise with `in use` if the target is the current directory, and otherwise replaces it.
6. `cp` checks the quota with the copied files added (and a replaced file's text no longer counted): `cp: DST: no space left`.
7. `mv` takes the source out of its directory and puts it in at the target, under the target's name (the source keeps its children; if it contains the current directory, the current directory moves with it). `cp` puts a copy of the source there (with `-r`, a copy of everything below it).

**`du [PATH]`** (zero or one path; none means `.`): a file prints `<size> <absolute path>`. A directory prints that line for itself and then for every directory below it (files get no line) in *tree order*: a directory, then the subtree of each of its children in byte order of their names.

**`find PATH`** or **`find PATH -name PATTERN`**: prints the absolute path of the resolved entry and every entry below it (files and directories), in tree order. With `-name`, only entries whose name matches the pattern are printed (the root's name is the empty text). In a pattern, `*` matches any sequence of characters (also none), `?` matches exactly one character, and every other character matches itself; the whole name must match. Three arguments whose second is not `-name`, or any other count but 1 or 3, is `find: usage`.

**`quota`** prints `<usage> <limit>`, or `<usage> none` when there is no limit. **`quota N`**, where N is 1 to 10 ASCII digits (leading zeros allowed), sets the limit to N; if the usage is greater than N, it prints `quota: N: below usage` (N as written) and the limit stays. Any other argument is `quota: usage`.

## Output

Every output line ends with a newline (a file's text already ends with one).
