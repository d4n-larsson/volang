# Data Model: Sleep

No new types. The clock has three modes (`src/rt_effects.c`, `src/replay.c`):

| Mode | Set by | `now_ms` | `sleep(ms)` |
|---|---|---|---|
| real | default | wall-clock ms | waits ≥ ms (monotonic), stops at the `--timeout` deadline (R0009) |
| fixed | `--clock-ms N`, every `vo test` | N, N+1, ... | no wait; the next reading is ms larger (R0001 on overflow) |
| replay | `vo replay` | recorded readings | no wait |

Validation: `ms < 0` is R0007 in every mode.
