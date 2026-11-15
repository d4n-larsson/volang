# Data Model: Small Language and Toolchain Fixes

## Recording: the `http` event (spec 006/007, extended)

| Field | Type | Note |
|---|---|---|
| `host` | str | as before |
| `port` | int | as before |
| `request` | str | **new**: the request line without the protocol, `METHOD TARGET` (`GET /status?x=1`); absent in recordings made before spec 009 |
| `result` | `"ok"` / `"err"` | as before |
| `value` | str | the raw response or the error text, as before |

In memory, `Event` gains `request` (offset 48, `EV_SIZE` 56). Replay matches kind, host, port and, when
the recorded event has one, `request`.

## Match arms

An arm `case NAME()` carries the flag `NF_EMPTY_ARM` (no binding, explicit empty parentheses), next to the
existing binding (`case NAME(v)`) and discard (`case NAME(_)`, `NF_DISCARD`) forms.

## The grant frame (run time)

For a running frame F, the grant frame is F itself if F's function declares its effects (or F is a test),
otherwise the grant frame of F's caller; `main` ends the walk. R0010 names the grant frame's function.
