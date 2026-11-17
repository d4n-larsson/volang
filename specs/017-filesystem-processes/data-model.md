# Data Model: Filesystem and Processes

## Effects

| Effect | Bit | Scope | Covers |
|---|---|---|---|
| console | 1 | — | unchanged |
| input | 2 | — | unchanged |
| file.read | 4 | directories | `read_file`; `std.fs.info`, `list_dir`, `walk` |
| file.write | 8 | directories | `write_file`; `std.fs.append_file`, `make_dir`; the new name of `rename` |
| file.delete | 16 | directories | **new**: `std.fs.delete`; the old name of `rename` |
| net | 32 | hosts | unchanged (was 16) |
| clock | 64 | — | unchanged (was 32) |
| random | 128 | — | unchanged (was 64) |
| env | 256 | names | unchanged (was 128) |
| serve | 512 | addresses | unchanged (was 256) |
| process | 1024 | programs | **new**: `std.process.run`, `run_with` |

`EF_ALL` = 2047. The bit order is the canonical order (`vo fmt`, `vo.effects`, reports).

### Scope rules

- **Directory scopes** (`file.read`, `file.write`, `file.delete`): normalized as today (no leading
  `./`, no empty or `.` components, one trailing `/`, `""` for the start directory); empty, absolute or
  `..` is E0506.
- **Program scopes** (`process`): non-empty; no NUL; no `..` component; no trailing `/`; a leading
  `./` is dropped. Three forms: bare name (`git`), relative path (`tools/gen`), absolute path
  (`/usr/bin/git`). Otherwise E0506 ("not a valid program: write a name, or a path without `..`").
- **Unscoped `process`** in a declaration outside `std/`: E0512.
- **Strictly inside**: `delete` and both ends of `rename` must lie below a scope directory, not be it.

## Values (std.fs)

```text
variant Info { missing; file(int); directory; other; }   # file carries its size in bytes
record Entry { name: str; info: Info; link: bool; }      # info is never missing
```

- `Entry.info` of a link: the target's kind when the target resolves inside one of the running grant's
  `file.read` scopes; `other` otherwise.
- A listing: entries sorted by the bytes of `name`; no `.` or `..`; names that are not valid UTF-8 left
  out.

## Values (std.process)

```text
record Output { code: int; stdout: str; stderr: str; }
```

- `code`: the exit status 0-255, or 128 + the signal number when the child was killed by a signal.
- `stdout`, `stderr`: everything the child wrote, each at most 16 MiB and valid UTF-8 (otherwise the
  call is an `err`).

## Primitive encodings (std-internal, also the recorded values)

| Primitive | `ok` text |
|---|---|
| `__fs(1, path, "")` info | `m` (missing), `f<size>`, `d`, `o` |
| `__fs(2, path, "")` list_dir | entries joined by `/`; each `<kind>[l] <name>` with kind as above (never `m`) |
| `__fs(3, path, text)` append_file | `""` |
| `__fs(4, path, "")` make_dir | `""` |
| `__fs(5, path, "")` delete | `""` |
| `__fs(6, from, to)` rename | `""` |
| `__run(program, args, input, env)` | `<code>\n<len of stdout>\n<stdout><stderr>` |

`err` texts are the fixed messages of contracts/codes.md.

## A program run

| State | Meaning | Leaves by |
|---|---|---|
| checked | program text inside the scopes of both grants (else R0010) | resolution |
| resolved | the file to execute (bare name: first `PATH` match) | fork, or `err("not found")` |
| started | child in its own process group, pipes 0/1/2 and the error pipe | the error pipe: errno → `err`; EOF → running |
| running | stdin written, stdout/stderr read, SIGCHLD watched | exit (reaped), the deadline, or the output limit |
| reaped | group killed, pipes drained without waiting | result: `ok(Output)` or an `err` (UTF-8, limit) |
| timed out | group killed and reaped | R0009 for the vo program |

What the child gets: `argv` = [program text, args...]; `envp` = exactly the given `NAME=value` strings
(none for `run`); working directory = the start directory; an empty signal mask; no other descriptor.

## Recording events (schema 1, contracts/recording.md)

| kind | Fields |
|---|---|
| `info`, `list_dir` | `path`, `result` (`ok` with `value`, `err` with `value`, `denied`) |
| `append_file`, `make_dir`, `delete` | `path`, `result` (`ok`, `err` with `value`, `denied`) |
| `rename` | `path`, `to`, `result` |
| `run` | `program`, `args` (list of str), `result` (`ok` with `value`: the `__run` text, `err` with `value`, `denied`) |

Not recorded: the text appended, the input given to a child, the environment given to a child.

## New code

| Code | Name | When | Exit |
|---|---|---|---|
| E0512 | `process_unscoped` | `uses process` without a scope outside the standard library | 65 |
