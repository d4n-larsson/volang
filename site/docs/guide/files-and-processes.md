# Files and processes

`read_file` and `write_file` read and replace whole files. For everything else on the file system,
`std.fs` lists directories, looks at paths, appends, creates directories, deletes and renames;
`std.process` runs other programs. Each needs an effect with a scope, so a reviewer reads in `main`'s
`uses` clause where a program may change files and which programs it may start.

## Directories and paths

`list_dir(path)` returns the entries of a directory, sorted by the bytes of their names, each with
what it is; `info(path)` says what is at one path. A missing path is a value (`Info.missing`), not an
error. Both need `file.read`, with a scope covering the path:

```vo
import std.fs.Info;
import std.fs.append_file;
import std.fs.info;
import std.fs.list_dir;
import std.fs.make_dir;

fn describe(i: Info) -> str {
    match i {
        case missing {
            return "missing";
        }
        case file(size) {
            return "file of " + str(size) + " bytes";
        }
        case directory {
            return "directory";
        }
        case other {
            return "something else";
        }
    }
}

fn main() -> result[void] uses console, file.read("notes/"), file.write("notes/") {
    try make_dir("notes/old");
    try append_file("notes/b.txt", "second\n");
    try append_file("notes/a.txt", "first\n");
    for e in try list_dir("notes") {
        print(e.name + ": " + describe(e.info));
    }
    print("notes/c.txt: " + describe(try info("notes/c.txt")));
    return ok();
}
```

```output
a.txt: file of 6 bytes
b.txt: file of 7 bytes
old: directory
notes/c.txt: missing
```

`walk(dir)` returns every file below a directory, sorted, as paths you can pass to `read_file`; it
does not follow links into directories. An entry that is a symbolic link has `link: true` and the kind
of its target when the target is inside your `file.read` scopes (`other` otherwise), so a listing
never tells you about files outside the grant. Names that are not UTF-8 are left out.

## Changing files

| Function | Effect | Fails with |
|---|---|---|
| `append_file(path, text)` | `file.write` | `"is a directory"`, `"permission denied"`, ... (creates a missing file) |
| `make_dir(path)` | `file.write` | `"not a directory"` (a file in the way), `"already exists"` (a file there) |
| `delete(path)` | `file.delete` | `"not found"`, `"not empty"` (only empty directories) |
| `rename(from, to)` | `file.delete` for `from`, `file.write` for `to` | `"already exists"` (nothing is overwritten), `"not found"` |

Deleting is its own effect: `file.write("out/")` lets a function create and replace files, not delete
them, so "gains file.delete" stands out when you review a change (see [Effects](effects.md)). `delete`
removes a link, never what it points to, and neither `delete` nor `rename` removes a scope directory
itself: with `file.delete("tmp/")` everything inside `tmp/` can go, `tmp/` stays. There is no
recursive delete; a program that must clear a tree walks it.

```vo
import std.fs.append_file;
import std.fs.delete;
import std.fs.make_dir;
import std.fs.rename;

fn main() -> result[void] uses console, file.write("tmp/"), file.delete("tmp/") {
    try make_dir("tmp");
    try append_file("tmp/draft.txt", "text\n");
    try rename("tmp/draft.txt", "tmp/final.txt");
    match rename("tmp/missing.txt", "tmp/final.txt") {
        case ok() {
            print("renamed");
        }
        case err(e) {
            print("rename: " + e);
        }
    }
    try delete("tmp/final.txt");
    print("done");
    return ok();
}
```

```output
rename: not found
done
```

Every path is checked at run time against your scopes ([R0010](../errors/R0010.md) outside them) and the operation then
stays beneath the scope directory: a symbolic link that another process swaps in cannot redirect it.

## Running programs

`run(program, args)` starts a program, waits for it to end, and returns its exit code and output.
`run_with(program, args, input, env)` also gives it text on standard input and environment variables
(`"NAME=value"`). There is no shell: each argument reaches the program exactly as written, so spaces,
quotes, `;` and `$(...)` mean nothing special.

```vo
import std.process.run;

fn main() -> result[void] uses console, process("echo") {
    let out = try run("echo", ["one two", "$(not run)"]);
    write(out.stdout);
    print("exit " + str(out.code));
    return ok();
}
```

```output
one two $(not run)
exit 0
```

- The `process` scope names each program: `"git"` is looked up through `PATH`, `"tools/gen"` or
  `"/usr/bin/git"` is that file. A scope is required ([E0512](../errors/E0512.md)), and the program you
  run must match one exactly: `process("git")` does not allow `run("/usr/bin/git", ...)`.
- A non-zero exit is an `ok` result with that `code`; a program killed by signal n has code 128 + n.
  `err` means it could not run (`"not found"`, `"permission denied"`) or its output could not be
  returned (over 16 MiB, or not UTF-8).
- The program runs in the directory your program started in, with only the environment you pass (none
  for `run`), no other open files, and no input after `input`. `--timeout` stops it and everything it
  started.
- **A started program is not confined.** It can do anything the user running `vo` can, outside your
  program's other scopes. Granting `process("git")` grants whatever `git` can do: grant the programs you
  would run by hand.

## Replay and tests

Listings, file information, results of changes and program outputs are inputs: `vo run --record`
saves them and `vo replay` feeds them back without touching a file or starting a program (see
[Determinism](determinism.md)). Tests declare the same effects, for example
`test "build" uses file.read("fixtures/"), process("tools/gen") { ... }`.
