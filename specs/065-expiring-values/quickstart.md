# Quickstart: Expiring values (spec 065)

Signatures and reports: [contracts/std-time.md](contracts/std-time.md). Rules:
[data-model.md](data-model.md).

Prerequisite: `make` (the native build at `bin/vo`). Work in an empty directory.

## 1. A token that must be asked for with the time (User Story 1)

`main.vo`: a function `header(token: time.Expiring[str], now: int) -> result[str]` that returns
`"Bearer " + try token.value_at(now)`, and a `main` that wraps `"secret"` with
`time.expiring("secret", 1000)` and prints the result of `header` at times 0, 999, 1000 and 5000 (a
`match` on each).

| Run | Expect |
|---|---|
| `vo check main.vo` | exit 0, no diagnostics; `header` has no `uses` clause |
| `vo run main.vo` | `Bearer secret` twice, then `expired at 1000, read at 1000` and `expired at 1000, read at 5000`; exit 0 |

## 2. The ways around are refused

Add, one at a time, each line of the misuse table in the contract.

| Run | Expect |
|---|---|
| `vo check main.vo` after each | exit 65 with the code in the table; for the dropped result, `vo check --json` carries the fix `try ` |

## 3. Durations and time left (User Story 2)

In a `test`: `let t = time.expiring_for("secret", 1000, 500);` then `expect_eq` on `t.expires_at`
(1500), `time.left_ms(t, 1200)` (300), `time.left_ms(t, 1500)` (0), and on
`t.value_at(1200 + 300)` (the error: a token needed for 300 ms more is expired).

| Run | Expect |
|---|---|
| `vo test main.vo` | all pass |
| a `main` that calls `time.expiring_for(1, 9223372036854775807, 1)` | R0001 Integer overflow, exit 70 |

## 4. Store and rebuild (User Story 3)

A record `Stored { token: str; expires_at: int; }`; build it from `try t.value_at(now)` and
`t.expires_at`, then `time.expiring(s.token, s.expires_at)`.

| Run | Expect |
|---|---|
| reads of the rebuilt value at 0, 1499, 1500 | the same three results as the original |

## 5. Replay (SC-003)

A `main` with `uses console, clock` that wraps a value for 100 ms at `now_ms()`, calls
`time.sleep(200)`, and reads at `now_ms()`.

| Run | Expect |
|---|---|
| `vo run --record run.rec main.vo` | prints the expiry error with two real times; exit 0 |
| `vo replay run.rec` | the same bytes, the same exit status, without waiting |

## 6. The suite

| Run | Expect |
|---|---|
| `make quick AREA=time` between edits | pass |
| `make test` once, when the change is complete | pass, including the documentation examples and the reference's token budget |
| `git diff --stat src/` | empty |
