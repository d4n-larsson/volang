# Contract: Recordings

`vo run --record FILE` writes JSON lines (schema 1): a header, one event per input the program received, in order, and an `end` record. `vo replay FILE` feeds the events back.

## Records

| kind | Fields |
|---|---|
| `recording` | `program` (entry file as given to `vo run`), `fingerprint` (16 hex digits, FNV-1a 64 over each program file's path and text in load order), `seed` (the seed actually used), `clock_ms` (the `--clock-ms` value or `null`), `args` (list of str) |
| `read_line` | `result` `"ok"` with `value`, or `"err"` with `value` `"eof"` |
| `read_all` | `value` |
| `read_file` | `path` (as the program passed it), `result` `"ok"` with `value`, `"err"` with `value` (the error text), or `"denied"` |
| `write_file` | `path`, `result` `"ok"`, `"err"` with `value`, or `"denied"` (the content is not recorded) |
| `env` | `name`, `result` `"ok"` with `value`, or `"err"` |
| `now_ms` | `value` (int) |
| `http` | spec 007: `host`, `port` (int), `request` (spec 009: the request line without the protocol, `"GET /status"`), `result` `"ok"` with `value` (the raw response), or `"err"` with `value` (the error text). The host is checked against the grant (R0010) before the event, in replay too; replay matches host, port and request (R0014 otherwise; events without `request`, from spec 007 recordings, match by host and port) and does not use the network |
| `listen` | spec 011: `address`, `result` `"ok"` with `value` (int, the listener) or `"err"` with `value` (the reason). The address is checked against the grant (R0010) first; replay opens no socket |
| `request` | spec 011: `value`, the raw bytes of a request the server received (`""`: the server was stopped). Replay feeds them to the program in order; the end of the recording stops the server |
| `response` | spec 011: `value`, the raw bytes of the response sent. Replay compares the new response with it: a difference is a note on stderr (`--json`: a `response_changed` record), never an error |
| `end` | `status` (the exit status of the run) |

```json
{"v":1,"kind":"recording","program":"main.vo","fingerprint":"9f2c1b0e5d3a7781","seed":1,"clock_ms":null,"args":["in.txt"]}
{"v":1,"kind":"env","name":"LIMIT","result":"ok","value":"3"}
{"v":1,"kind":"read_file","path":"in.txt","result":"ok","value":"a\u000ab\u000a"}
{"v":1,"kind":"read_line","result":"err","value":"eof"}
{"v":1,"kind":"end","status":70}
```

- `random_int` needs no events: it is fixed by `seed`.
- Strings are JSON strings: `\"` and `\\` escaped, control bytes as `\u00XX`. A text value that is not valid UTF-8 also has every byte >= 0x80 as `\u00XX` and `"bytes":true` on its record. Every `\u00XX` stands for one byte.
- An event must have what replaying it needs (a path or name, a result, a string value, or an integer for `now_ms`); otherwise the recording is T0008.
- The recording is complete however the run ends: normal end, `exit`, runtime error, timeout, out of memory. A run killed by a signal leaves no `end` record, and replaying it is T0008.
- It contains the environment variables and file contents the program read; treat it like those.

## Replay

- The program is loaded from `program` (relative to the working directory). If its fingerprint differs, a note is printed and the run continues (contracts/reports.md).
- `seed`, `clock_ms` and `args` come from the header.
- Each input builtin takes the next event. If the kind differs, or the `path`/`name` differs, or there are no events left: R0014, naming the input asked for and the event recorded (or "the recording has no more inputs").
- `write_file` writes nothing and returns the recorded result; `denied` in `read_file`/`write_file` raises R0010 as in the recorded run, without looking at the filesystem.
- Leftover events at the end of a replay are not an error (a fixed program may read less).
- The replayed run's stdout, stderr and exit status equal the recorded run's when the program is unchanged and the recorded run did not time out. A timed-out run replays to a timeout too (exit 124), but the output before it depends on the machine's speed.
