# Data Model: std.web (spec 029)

## Application (`App[C, E]`, built by the program)

| Field | Meaning |
|---|---|
| routes | `list[Route[C, E]]`: operations, health, info; one table per application |
| steps | `list[Step[C, E]]`: around every request whose path starts with the step's prefix, in order |
| errors | `list[ErrorStatus]`: error kinds beside the defaults (R4); a repeated kind overrides |

Validated when served (`serve_app`) or first handled: patterns parse, no two routes of one method match the
same paths (the checks of R6 that `vo check` could not make).

## Route (`Route[C, E]`, opaque)

method (GET, POST, PUT, PATCH, DELETE), pattern (segments: literal or variable), and the operation wrapped
with its binding: `fn(C, Request, map[str, str]) -> Response uses E` (the path variables matched).

## Operation input (`I`, a record of the program)

Each field bound by source (contracts/web.md); limits and defaults from its annotations
(contracts/json.md). Binding outcome: the record, or the list of problems (field path, reason) and
whether only limits failed (422) or the input was malformed (400).

## Step (`Step[C, E]`)

prefix (a path prefix, `/` for all) and `run(ctx, request, next)`. The chain for a request is the steps
whose prefix matches, in list order, ending at routing.

## Error status (`ErrorStatus`)

kind (text before the first `: ` of an `err`) → status. Defaults: bad_request 400, unauthorized 401,
forbidden 403, not_found 404, conflict 409, gone 410, unprocessable 422.

## Problem (an answer body)

type (`about:blank`), title (the status's reason phrase), status, detail (optional), errors (optional list
of {field, reason}).

## Setting record (`T` of `load_settings`)

Any `json` record: its fields' `@default` make settings optional, their limits check values; sources in
order: the settings file (a JSON object), then environment variables `PREFIX_FIELD`.

## Flow of one request

```text
serve_workers worker: handle_request(app, ctx, request)
  -> steps (matching prefix, list order) -> routing (404 / 405 / HEAD / OPTIONS)
  -> binding (400 / 415 / 422) -> operation(ctx, input) -> ok(Response) | err("kind: detail")
  -> error mapping (status + problem) -> back out through the steps
```
