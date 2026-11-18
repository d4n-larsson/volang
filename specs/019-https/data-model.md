# Data Model: HTTPS

## URL (std.http, extended)

| Field | Rule |
|---|---|
| scheme | `http` or `https`, from the URL prefix (any case); anything else is the existing "not an http:// URL" err (message now "not an http:// or https:// URL") |
| host | as today; checked against `net` scopes (E0503 literal, R0010 run time) |
| port | explicit, else 80 (`http`) or 443 (`https`) |
| path | as today |

## Secure connection (src/tls.c, one per request)

States: resolving → connecting → handshaking → verified → request sent → reading → closed.
Every state change that waits does so under the `--timeout` deadline. Failure in any state before
"verified" gives an R7 `err` and sends no request bytes. Memory: TLS arena; freed at close.

## Trust anchors (per process)

| Source | When loaded | Failure |
|---|---|---|
| machine store (first existing of three paths) | first HTTPS request | none found: every HTTPS request is the "no trusted certificate authorities" err |
| `--ca-file FILE` (repeatable) | startup, before the program | unreadable: T0002; no certificate: T0013 |

Parsed once into TLS memory; inherited by forked tasks; never recorded.

## Verification outcome

One of: ok, not_trusted, self_signed, wrong_host, expired, not_yet_valid, no_acceptable_protocol,
tampered, handshake_failed — chosen by fixed precedence from the library's flags (research R7), each
with one message.

## http event (recording schema 1)

host, port, request line, result — plus optional `scheme: "https"`. Replay matches all four.
