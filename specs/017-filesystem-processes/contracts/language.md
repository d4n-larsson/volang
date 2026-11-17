# Contract: Language and Library

## Effects

Two effects join the table and the canonical order: console, input, file.read, file.write,
**file.delete**, net, clock, random, env, serve, **process**.

| Effect | Covers | Scope |
|---|---|---|
| file.delete | `std.fs.delete`, and `std.fs.rename` (it removes the old name) | directories, like `file.write` |
| process | `std.process.run`, `run_with` | programs: a name found through `PATH` (`"git"`), or a relative or absolute path (`"tools/gen"`, `"/usr/bin/git"`); required (E0512) |

- `file.write` alone never deletes or renames. `rename(from, to)` needs `file.delete` covering `from`
  and `file.write` covering `to`.
- `delete` and `rename` refuse the scope directory itself (R0010): `file.delete("tmp/")` deletes inside
  `tmp/`, never `tmp/`.
- A program's text must match a `process` scope exactly: `process("git")` allows `run("git", ...)`, not
  `run("/usr/bin/git", ...)` or `run("./git", ...)`. A leading `./` is dropped only while a `/` remains
  (`"./bin/x"` is `"bin/x"`; `"./x"` stays a file in the start directory, unlike `"x"`).
- Deleting and renaming act on a link itself, never on its target: the last component of the path is
  not followed when it is checked.

## std.fs

```vo
# What is at a path: missing, a file with its size in bytes, a directory, or something else.
export variant Info { missing; file(int); directory; other; }

# One directory entry. A link has the kind of its target when the target is inside the caller's
# file.read scopes, otherwise `other`.
export record Entry { name: str; info: Info; link: bool; }

export fn info(path: str) -> result[Info] uses file.read
export fn list_dir(path: str) -> result[list[Entry]] uses file.read      # sorted by name bytes, no . or ..
export fn walk(dir: str) -> result[list[str]] uses file.read             # every file below dir, sorted, as dir + "/" + relative path; links are not followed into
export fn append_file(path: str, text: str) -> result[void] uses file.write   # creates the file if missing
export fn make_dir(path: str) -> result[void] uses file.write            # with missing parents; ok if it exists as a directory
export fn delete(path: str) -> result[void] uses file.delete             # a file, a link, or an empty directory
export fn rename(from: str, to: str) -> result[void] uses file.write, file.delete   # an existing `to` is an err; nothing changes
```

Error texts (`err` values): `not found`, `already exists`, `not empty`, `not a directory`,
`is a directory`, `permission denied`, `no space left`, `on another file system`,
`read-only file system`, `not supported on this file system`, `cannot move a directory into itself`
(rename), `error N` (any other system error N).
`info` returns `ok(Info.missing)` where the others say `not found`.

## std.process

```vo
# What a finished program returned: its exit code (128 + n when killed by signal n) and its output.
export record Output { code: int; stdout: str; stderr: str; }

# Runs program with args (no shell), no input and an empty environment, and waits for it.
export fn run(program: str, args: list[str]) -> result[Output] uses process

# The same, with text for its standard input and environment variables as "NAME=value".
export fn run_with(program: str, args: list[str], input: str, env: list[str]) -> result[Output] uses process
```

- A non-zero exit is `ok` with that `code`; `err` only when the program could not run or its output
  could not be returned: `not found`, `permission denied`, `cannot run: error N`,
  `output is larger than 16 MiB`, `output is not UTF-8`.
- R0007: an empty program name, a NUL byte in the program or an argument, an `env` entry that is not
  `NAME=value` with a non-empty name.
- The child runs in the directory the vo program started in, with only the given environment
  (passing a variable does not need `env`; reading one does), no other open files, and no input after
  `input`. `--timeout` stops it and everything it started (R0009); so does the end of the vo program.
- **A started program is not confined**: it can do anything the user running `vo` can, outside the
  program's file, network and environment scopes. Granting `process("git")` grants what `git` can do.

## Reference text (shipped in `docs/reference.md`, ~294 tokens: 11,393 → 11,687)

Effects table rows:

```text
| file.delete | `std.fs.delete` and `rename` |
| process | `std.process.run` and `run_with` (running other programs) |
```

Scopes bullet: "directories for `file.read`/`file.write`/`file.delete` ..., programs for `process`
(`"git"`: found through `PATH`; `"tools/gen"`, `"/usr/bin/git"`: that file; required, E0512; matched
exactly)." The file effects row also names `std.fs`.

Canonical order line: "console, input, file.read, file.write, file.delete, net, clock, random, env,
serve, process; scopes sorted."

Modules table rows:

```text
| std.fs | `info`, `list_dir`, `walk` (file.read); `append_file`, `make_dir` (file.write); `delete`, `rename` (file.delete) |
| std.process | `run`, `run_with`: run a program to its end, no shell (process) |
```

Two bullets and an example (in "Standard library and packages"; the second bullet: "File operations
fail with `err` values (...); `info` reports `missing` as a value. `delete` and `rename` never remove a
scope directory itself."):

> `std.process.run(program, args)` runs a program with no shell (arguments are passed as they are),
> no input and an empty environment, waits for it, and returns `Output { code, stdout, stderr }`;
> `run_with` adds input text and `NAME=value` variables. A non-zero exit is `ok`; killed by signal n,
> `code` is 128 + n. `--timeout` covers the program and everything it starts. A started program is not
> confined: `process("git")` grants whatever `git` can do. File operations fail with `err` values
> (`"not found"`, `"already exists"`, `"not empty"`, ...); `info` reports `missing` as a value.

```vo
import std.fs.info;
import std.process.run;

fn main() -> result[void] uses console, file.read("./"), process("echo") {
    match try info("no-such-file.txt") {
        case missing {
            print("missing");
        }
        case file(size) {
            print(str(size));
        }
        case directory {
            print("directory");
        }
        case other {
            print("other");
        }
    }
    let out = try run("echo", ["a b", "$HOME"]);
    write(out.stdout);
    print(str(out.code));
    return ok();
}
```

```output
missing
a b $HOME
0
```
