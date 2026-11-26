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
import std.fs;

fn describe(i: fs.Info) -> str {
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
    try fs.make_dir("notes/old");
    try fs.append_file("notes/b.txt", "second\n");
    try fs.append_file("notes/a.txt", "first\n");
    for e in try fs.list_dir("notes") {
        print(e.name + ": " + describe(e.info));
    }
    print("notes/c.txt: " + describe(try fs.info("notes/c.txt")));
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
import std.fs;

fn main() -> result[void] uses console, file.write("tmp/"), file.delete("tmp/") {
    try fs.make_dir("tmp");
    try fs.append_file("tmp/draft.txt", "text\n");
    try fs.rename("tmp/draft.txt", "tmp/final.txt");
    match fs.rename("tmp/missing.txt", "tmp/final.txt") {
        case ok() {
            print("renamed");
        }
        case err(e) {
            print("rename: " + e);
        }
    }
    try fs.delete("tmp/final.txt");
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
`run_with(program, args, input, env, dir)` also gives it text on standard input, environment variables
(`"NAME=value"`) and a working directory: `"./"` is the directory vo started in (where `run` starts
programs); any other directory must lie inside your `file.read` scopes, and is `err("directory not
found")`, `err("not a directory")` or `err("directory not accessible")` when the program cannot start
there. A program written with a `/` (`"tools/gen"`) is still found from the start directory. There is no
shell: each argument reaches the program exactly as written, so spaces,
quotes, `;` and `$(...)` mean nothing special.

```vo
import std.process;

fn main() -> result[void] uses console, process("echo") {
    let out = try process.run("echo", ["one two", "$(not run)"]);
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

## Large inputs

`read_file`, `std.http.get` and `run` return everything at once, so the whole file, body or output has
to fit in memory (and a body in 16 MiB). For anything larger, or anything you want to see while it
arrives, four functions hand you one piece at a time. You pass a function; it is called once per piece
and answers `ok(true)` to go on, `ok(false)` to stop early, or an error, which becomes the result.
Nothing is left open when the call returns, and memory does not grow with the size of the source.

`each_line(path, f)` reads a text file line by line, and `each_chunk(path, size, f)` reads any file in
pieces of `size` bytes. What the function wants to keep goes into a record or list it captures:

```vo
import std.fs;

record Count {
    lines: int;
    errors: int;
    size: int;
}

fn main() -> result[void] uses console, file.read("./"), file.write("./") {
    try write_file("app.log", "start\nerror: disk full\nretry\nerror: disk full\nstop\n");
    let count = new Count { lines: 0, errors: 0, size: 0 };
    try fs.each_line("app.log", fn(line) {
        set count.lines = count.lines + 1;
        if find(line, "error") == 0 {
            set count.errors = count.errors + 1;
        }
        return ok(count.errors < 2);
    });
    print(str(count.errors) + " errors in the first " + str(count.lines) + " lines");
    try fs.each_chunk("app.log", 16, fn(piece) {
        set count.size = count.size + len(piece);
        return ok(true);
    });
    print(str(count.size) + " bytes");
    return ok();
}
```

```output
2 errors in the first 4 lines
51 bytes
```

`run_lines(program, args, input, env, dir, f)` runs a program like `run_with` and calls the function with
each line of its standard output as soon as the program has written it, so a long build shows its
progress and can be stopped at its first error:

```vo
import std.process;

fn main() -> result[void] uses console, file.read("./"), process("sh") {
    let script = "echo compiling; echo linking; sleep 5; echo done";
    let out = try process.run_lines("sh", ["-c", script], "", new list[str], "./", fn(line) {
        print("> " + line);
        return ok(line != "linking");
    });
    print("exit " + str(out.code));
    return ok();
}
```

```output
> compiling
> linking
exit 137
```

`std.http.send_stream` takes the arguments of `send` and a function that gets the response's status
and headers with each piece of the body, at most 64 KiB at a time, with no limit on the whole:

```vo
import std.fs;
import std.http;

fn main() -> result[void] uses console, file.write("downloads/"), net("example.com") {
    let head = try http.send_stream("GET", "https://example.com/big.tar", new list[http.Header], "", 30000, fn(head, piece) {
        if head.status != 200 {
            return ok(false);
        }
        try fs.append_bytes("downloads/big.tar", piece);
        return ok(true);
    });
    print("status " + str(head.status));
    return ok();
}
```

- The function's effects are yours to declare: the first example needs nothing more, the second needs
  `console` because its function prints.
- A line comes without its line end (`\n` or `\r\n`). A line that is not UTF-8, or is longer than
  16 MiB, ends the read with `err("line N: ...")`. `each_chunk` takes a size from 1 to 16 MiB.
- Only regular files can be read in pieces; a pipe is `err("not a regular file: read it whole with
  read_file")`.
- Stopping `run_lines` kills the program (its code is then 137, unless it had just ended by itself). When the output ends by itself,
  `run_lines` waits for the program and returns its code and standard error. There is no time limit
  per line: a silent program is bounded by `--timeout`.
- `send_stream`'s time limit bounds each wait for the server, not the whole transfer. Redirects are
  returned, not followed.
- A recording holds every piece that was delivered, so it is as large as what was read.
- Writing is already piece by piece: `append_file` and `append_bytes`. Standard input too: `read_line`.

## Replay and tests

Listings, file information, results of changes and program outputs are inputs: `vo run --record`
saves them and `vo replay` feeds them back without touching a file or starting a program (see
[Determinism](determinism.md)). Tests declare the same effects, for example
`test "build" uses file.read("fixtures/"), process("tools/gen") { ... }`.
