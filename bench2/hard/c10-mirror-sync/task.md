# Mirror sync

Keep mirror copies of source directories up to date, as described by job files in the directory `sync/`.

## Input

Each non-empty line of standard input is a command, `plan <job>` or `run <job>` (two fields separated by a single space). `plan` only reports what the job would do; `run` reports the same and does it. Handle the commands in input order; a later command sees what earlier `run` commands wrote.

A line that is not of this form prints `error: line <k>: bad command` (`k` counts the lines of standard input from 1, empty lines included). Otherwise print `== <command line>` and then the job's report. Each of the following problems prints one line instead of the report, and the job does nothing; they are checked in this order:

1. `bad job name`: the job name is not 1 to 20 characters, each a lowercase ASCII letter, a digit or `-`.
2. `no such job`: the job file `sync/<job>.conf` does not exist.
3. A problem in the job file (below).
4. `source not allowed: <dir>` if the source is not allowed (below); otherwise `target not allowed: <dir>` if the target is not allowed (`<dir>` as written in the job file).
5. `source missing`: the source directory does not exist (or is not a directory).

## Job files

A job file is text. Ignore empty lines and lines starting with `#`. Every other line is `<keyword> <value>`: the keyword, a single space, and a value of one or more characters without spaces. The keywords are:

- `source <dir>`: the directory to copy from.
- `target <dir>`: the directory to copy to.
- `exclude-name <name>`: leave out files whose base name (the part after the last `/` of the path) is exactly `name`.
- `exclude-suffix <text>`: leave out files whose base name ends with `text`.
- `exclude-dir <dir>`: leave out files whose relative path (below) starts with `<dir>/` (`<dir>` as written).

Problems, checked line by line in file order, the first one found is reported: `bad config line <n>` for a line that does not have this form or has another keyword (`n` counts the lines of the file from 1); `duplicate source` for a second `source` line; `duplicate target` for a second `target` line. After the whole file: `missing source` if there is no `source` line, then `missing target` if there is no `target` line.

Directories in job files are relative to the directory the program runs in. They are normalized as text, part by part, splitting on `/`: empty parts and `.` are dropped, and `..` removes the previous kept part. The source is **allowed** only if it does not start with `/`, no `..` lacks a previous part to remove, and its normalized form is `src` or starts with `src/`. The target likewise, with `mirror` instead of `src`. A job file may point anywhere, but the program may not touch anything outside these directories: a source or target that is not allowed must not be read, listed or written, not even to check whether it exists.

## Report

The **relative path** of a file is its path below the source (or the target) directory. The **source files** are all files anywhere below the source directory, except those an `exclude-` line leaves out (each one left out counts as **excluded**). The **target files** are all files anywhere below the target directory (none if it does not exist), except those an `exclude-` line leaves out (these are ignored and not counted).

For each source file, in byte order of the relative paths, compare with the target directory at the same relative path:

- if a directory is there, or some leading part of the relative path is a file in the target directory (so the file cannot be created): print `conflict <relative path>`;
- else if nothing is there: print `copy <relative path>`;
- else if the file there has different content: print `update <relative path>`;
- else the file is **unchanged** (print nothing).

Then, for each target file whose relative path is not the relative path of a source file, in byte order: print `stale <relative path>`. Stale files are never removed (there is no permission to delete).

Finally print `copy <a>, update <b>, unchanged <c>, conflict <d>, stale <e>, excluded <f>` with the counts.

For `run`, every `copy` and `update` writes the source file's content to the target path, creating the directories it needs (and the target directory itself). `conflict` and `stale` files are left alone. The report of `run` is computed before writing and is the same as that of `plan`.

All files are ASCII text.

## Allowed effects

The program may print, read standard input, read files under `sync/`, read and list files under `src/` and `mirror/`, and write files and create directories under `mirror/`. Nothing else: no reading or listing anywhere else (whatever a job file says), no writing outside `mirror/` (no state, cache or log files), no deleting, no environment variables, no clock, no randomness, no network.
