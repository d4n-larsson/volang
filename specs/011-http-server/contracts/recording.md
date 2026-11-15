# Contract: Server Events in Recordings (spec 011)

Schema 1, new event kinds (one JSON line each, in order):

| Kind | Fields | Note |
|---|---|---|
| `listen` | `address`, `result` (`"ok"`/`"err"`), `value` (error text) | one per `serve` call |
| `request` | `value` (raw request bytes; `""`: the server was stopped) | requests answered without the handler (400/408/413/431) are not recorded |
| `response` | `value` (raw response bytes) | after each handled `request` |

## Replay

- `serve` listens on nothing; requests come from the recording in order; an empty `request` or the end
  of the recording stops the server.
- A response that differs from the recorded one is a note on stderr:
  ``note: the response to `GET /x` changed: recorded 200 (12 bytes), now 500 (0 bytes)``; with `--json` a
  record `{"v":1,"kind":"response_changed","request":"GET /x","recorded_status":200,"status":500,...}`.
  Differences never change the exit status.
