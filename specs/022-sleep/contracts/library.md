# Contract: `std.time.sleep`

`sleep(ms: int)` uses clock

| Situation | Result |
|---|---|
| real clock | returns after at least `ms` ms; no CPU while waiting |
| fixed clock (`--clock-ms`, `vo test`) | returns at once; the next clock reading is `ms` larger than it would have been |
| `vo replay` | returns at once |
| `ms == 0` | returns at once |
| `ms < 0` | R0007 "`sleep` needs a duration of at least 0 ms, got N." at the call |
| `--timeout` deadline during the wait | R0009 at the deadline, at the call |
| fixed clock past the largest int | R0001 |
| Ctrl-C / SIGTERM | the program ends as at any other time (inside `serve`: after the handler) |

`vo doc` example (fixed clock): `let a = now(); sleep(500); now() - a is 501`.
Reference: the `clock` row names `std.time.sleep`; the `--clock-ms` sentence says sleeps then do not wait.
