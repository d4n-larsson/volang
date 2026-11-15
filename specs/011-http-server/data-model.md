# Data Model: HTTP Server Support in `std.http`

## Request (new, `std.http`)

| Field | Type | Note |
|---|---|---|
| `method` | str | as sent (`GET`, `POST`, ...) |
| `path` | str | the target before `?` (`/v2/status`), not decoded |
| `query` | str | the target after `?`, without it; `""` when there is none |
| `headers` | list[Header] | in order, names as sent |
| `body` | str | the whole body (chunked bodies decoded); `""` when there is none |

## Response (existing, `std.http`)

`status: int`, `headers: list[Header]`, `body: str`. As a server answer, `Content-Length` and
`Connection: close` are added unless present; the reason phrase is the standard one for the status.

## Effect set

`serve` is bit 256, after `env` in the canonical order. Scope: an address, `"PORT"` (loopback) or
`"0.0.0.0:PORT"` (all interfaces), PORT 1-65535.

## Server run (in a recording)

```text
listen(address, result) · (request(raw) · response(raw))* · request("")?
```

A request answered without the handler (`400`, `408`, `413`, `431`) is not recorded: replaying it would
not reach the program. An empty `request` is the stop.

## States of a server

`starting` → (`listen` fails) → `serve` returns `err` · `starting` → `waiting` ⇄ `handling` →
(signal or end of recording while `waiting`) → `stopped`: socket closed, signal mask restored, `serve`
returns `ok()`.
