# Contract: Codes

## New

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0512 | `process_unscoped` | 65 (check) | no | `uses process` without a scope outside the standard library; message: "`process` needs the programs it may run: write `process(\"NAME\")`." (no fix: which program is the reviewer's choice) |

Catalog entry (docs/errors.md): an example `fn main() uses process { ... }` and the fixed version
`uses process("git")`, with one sentence on why: an unscoped grant would allow every program on the
machine. Golden cases: `tests/cases/effects/process_unscoped`, `tests/cases/effects/explain_e0512`. The standard library's own declarations
(`std/process.vo`) are exempt: the caller's scopes apply.

## Existing codes, new triggers

| Code | New trigger |
|---|---|
| E0501 | a function calls a `std.fs` or `std.process` function without declaring its effect (including `delete`/`rename` with only `file.write`) |
| E0503 | a literal path argument of a `std.fs` function outside the declared scopes (for `rename`: `from` against `file.delete`, `to` against `file.write`); a literal program outside the `process` scopes (exact match) |
| E0504 | a function gains `file.delete` or `process`, or a wider scope of either |
| E0506 | an invalid `file.delete` scope (as `file.write`); an invalid `process` scope: "\"$1\" is not a valid program: write a name, or a path without `..`." |
| E0509 | message lists the eleven effects: "... use console, input, file.read, file.write, file.delete, net, clock, random, env, serve or process." (golden outputs of existing E0509 cases change) |
| R0007 | `std.process`: empty program, NUL in the program or an argument, a malformed `NAME=value`; `std.fs`: empty path or NUL byte |
| R0009 | a child still running at the deadline (it and its process group are killed first) |
| R0010 | any `std.fs` path outside the running or root grant's scopes for its effect, or the scope directory itself for `delete`/`rename`; a program not matching a `process` scope; JSON fields as today (`effect`, `target`, `allowed`, `declared_by`) |
| R0014 | replay: the next event differs in kind, path, `to`, program or arguments |

## Fixed `err` texts

`std.fs`: `not found`, `already exists`, `not empty`, `not a directory`, `is a directory`,
`permission denied`, `no space left`, `on another file system`, `read-only file system`,
`not supported on this file system`, `cannot move a directory into itself`, `error N`.

`std.process`: `not found`, `permission denied`, `cannot run: error N`, `output is larger than 16 MiB`,
`output is not UTF-8`.

Each text has a golden case, except `no space left`, `on another file system`, `read-only file system`,
`not supported on this file system` and the `error N` forms, which a test cannot produce on every
machine; those come from the same errno table and are checked by review.
