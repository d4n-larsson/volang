# Contract: Behavior Changes (spec 013)

## Grants

- An effect is allowed only if the running function's grant and the root grant (`main`'s clause; the
  test's or property's clause in `vo test`) both allow it; otherwise R0010 naming the grant that refused.
- A function value inside any type (list, map, result, record, variant, generic argument) carries its
  effects: passing it where those effects are not declared is a check error.
- A literal path, variable name or serve address outside the caller's scopes is E0503.

## Files

- A path with a NUL byte: R0010.
- If the start directory cannot be determined, every scoped file access: R0010.
- Relative paths are relative to the directory the program started in, even if it is renamed.
- A scoped access never leaves its scope: a symbolic link (absolute, or leading out, or swapped while
  running) that would take it out gives R0010. Relative links inside the scope work.
- Only regular files are read as source, packages, recordings or manifests.

## Imports and packages

- An import or package file reached through a link leading outside the program's tree or the package's
  directory: E0211.
- A package loads exactly the bytes its pin was computed from; an unreadable directory in a package: E0702.

## HTTP

- Request and response heads must be ASCII (no NUL, no bare CR): otherwise 400 (server) or `err` (client).
- Bodies are decoded once; a request is at most 64 KiB of head and framing plus 16 MiB of body (431/413).
- Request parsing runs under the handler's guard and time limit: any failure is a 500, never the end of
  the server.
- A response header name that is not a token, or a name or value with CR, LF or NUL: 500.

## Replay

- The recorded program must be a relative `.vo` path without `..`, naming a regular file: otherwise T0008.
- Every file event is checked against the grant (lexically): outside is R0010, as in the recorded run.
- A malformed literal, an overflowing integer, or a recording larger than `--max-memory`: T0008.

## Runtime

- `sort_by` whose list changes during the key calls: R0007.
- Equality and rendering of values too deep for the stack: R0005.
