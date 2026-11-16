# Data Model: Security Fixes from Review 1

## Grants at run time

| Grant | What it is | Set where |
|---|---|---|
| Root grant | the declared set a person reviewed: `main`'s clause (`vo run`, `vo replay`), the test's or property's clause (`vo test`) | at the start of the run, of each test, of each property case |
| Running grant | the frame's: its declaration (concrete part), its caller's for library and anonymous functions and for an effect parameter's part | `vm_push` |

An effect is allowed when both allow it. A requirement set (inferred effects, "any scope" bits) is never a
grant.

## Opened files

`start_fd`: the start directory, opened once. For a scoped access: `scope_fd` (the scope directory, opened
beneath `start_fd`) and `rel` (the path relative to it); the open is `openat2(scope_fd, rel, RESOLVE_BENEATH)`.

## HTTP message (framing result)

`head` (ASCII text up to the blank line, at most 64 KiB including chunk framing) and `body` (decoded bytes,
at most 16 MiB).

## Findings ledger

`docs/security-review-1.md`: each finding gains `status` (fixed) and `case` (the path of its case).
