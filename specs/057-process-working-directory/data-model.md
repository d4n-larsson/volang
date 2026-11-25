# Data Model: Process Working Directory

## Run (one start of a program)

| Field | Meaning | Rules |
|---|---|---|
| program | as written: a bare name (found through `PATH`) or a path with `/` (from the start directory) | must match a `process` scope exactly (unchanged) |
| arguments | passed as they are, no shell | no NUL byte (R0007, unchanged) |
| input | standard input text or bytes | unchanged |
| vars | `NAME=value` entries, the whole environment | unchanged |
| dir | **new**: the working directory, as written; `"./"` is the start directory | non-empty, no NUL byte (R0007); inside a `file.read` scope of every scoped frame on the call path (R0010; E0503 for a literal at check time); must exist and be a directory the user can enter (`err`) |

Order of checks: arguments (R0007) → `process` grant → `file.read` grant of `dir` (R0010) → the directory
is opened (`directory not found`, `not a directory`) → the program is found (`not found`) → the child
enters the directory (`directory not accessible`) → `execve` (`not found`, `permission denied`).

## Run event (recording)

| Field | Meaning |
|---|---|
| `kind` | `run` (unchanged) |
| `program` | as written (unchanged) |
| `args` | as written (unchanged) |
| `dir` | **new, optional**: the directory as written; absent means `"./"` (and is never written for `"./"`) |
| result | the recorded result (unchanged) |

Replay matches `program`, `args` and `dir`; any difference is R0014.

## Library change row (kind `arg`, new)

| Field | Value |
|---|---|
| module | `process` |
| old | `run_with`, `run_with_bytes`, `run_lines` |
| kind | `LC_ARG`: the call has one argument fewer than the function takes |
| call | the inserted argument text: `"./"` |
| factor | the index it is inserted at: 4 |
| since | 0.3.0 |

## E0501 fix scope (changed)

For each file effect a function lacks: when every need for it comes from literal targets of std
functions and builtins, the fix adds the effect with scopes: a file target's directory
(`read_file("data/a.txt")` → `"data/"`, `read_file("a.txt")` → `"./"`), a directory target as written
(`run_with(..., "jobs/")` → `"jobs/"`). Otherwise the effect is added unscoped, as before.

| Target | Kind |
|---|---|
| `read_file`, `write_file`, `std.fs.info`, `append_file`, `delete`, `rename` (both paths) | file |
| `std.fs.list_dir`, `walk`, `make_dir`, the `dir` of `run_with`, `run_with_bytes`, `run_lines` | directory |

These are the literal targets the checker records today (builtins in `fx_builtin_literal`, std functions
in `std_targets`) plus the three new rows. A call of another file function (`read_bytes`, `each_line`,
...) is a non-literal need, so a function using one keeps the unscoped fix; widening the target table is
not part of this spec.
