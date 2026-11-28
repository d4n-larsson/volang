# Research: Expiring values

Every decision below was tried on 2026-11-28 with `bin/vo` (the x86-64 build of commit 60b9d2d) on a
prototype of the type written as an ordinary program; the outputs quoted are that build's.

## R1. How the value is hidden (FR-003, FR-007)

**Decision**: the value lives in a closure. The record has two fields: the expiry moment and a
function from a time to a result. The function is built inside `std.time` and captures the value.

```vo
export record Expiring[T] {
    expires_at: int;
    value_at: fn(int) -> result[T];
}
```

**Rationale**: vo records have no private fields, so a field holding the value could be read as
`e.value` from anywhere. A captured variable has no name outside its function: the only thing a program
can do with `value_at` is call it with an int. The checker already refuses everything else, with no
change:

| Attempt | Report |
|---|---|
| `let s: str = t;` | E0301 Expected `str`, found `Expiring[str]` |
| `t.value_at(5);` with the result dropped | E0404, fix: insert `try ` |
| `t == t` | E0308 it holds a function, and functions cannot be compared |
| `format("{}", t)`, `str(t)` | E0301 |
| `json.to_json(t)` | E0328 `value_at` is a function |

**Alternatives considered**:

- *A record with a `value` field and a rule in the documentation.* Rejected by FR-007: a convention is
  what programs have today.
- *The same record plus a warning when code outside `std.time` reads `value`.* A checker rule and a new
  code for something the closure gives for nothing.
- *Private record fields.* A language feature (syntax, a checker rule, reference tokens) that no other
  requirement asks for.
- *A builtin type in C, like `result`.* Several hundred lines in the checker, compiler and VM.

**Costs, accepted**: an `Expiring[T]`, and any record that holds one, cannot be compared with `==`,
turned into JSON or sent to a worker. Each is a check error that names the function field. User Story 3
(keep the value and the moment, wrap again) is the way around all three and goes in the guide.

**Not hidden**: a runtime error trace prints the locals of each frame, and the frame of `expiring`
holds the value in the clear (`value: int = 1`). The record itself prints as
`value_at: <fn(int) -> result[str]>`. Traces already show every local of every frame; this spec does
not change what a trace shows.

## R2. The read is the field, not a function (Principle I)

**Decision**: the read is written `token.value_at(now)`. There is no `time.read(token, now)`.

**Rationale**: the field must exist and calling it is already valid vo, so a library function beside it
would be a second spelling of the same thing.

## R3. The error is a `str` (FR-004)

**Decision**: `value_at` returns `result[T]`; the error is the text `expired at 1500, read at 1700`
(both in milliseconds, as given).

**Rationale**: with a record `Expired { expired_at; now }` as the error type, `try` inside a function
that returns `result[T]` is E0412 ("convert it with `map_err`"), and most functions in a service
return `result[T]`. The read has one way to fail, so a structured error tells a caller nothing it does
not have: it holds `expires_at` and it passed `now`. A function that wants to branch on expiry matches
on `err(_)`.

**Alternatives considered**: the record error with an `expired_text` function (two more exported names
and a `map_err` at most call sites); calendar text in the message (longer, and tests run at clock 0,
where `1970-01-01T00:00:00.000Z` says less than `0`).

## R4. Where it lives: `std.time`

**Decision**: one record and three functions added to `std/time.vo`.

**Rationale**: a new module costs a row in the reference's library table, an entry in `STD_FILES` and
an entry in `doc_modules` in `src/doc.c`. In `std.time` it costs none of them, `src/` does not change
(SC-005), and the reference grows only by a few words in the existing row (about 25 tokens; SC-004
allows 150). Expiry moments are `std.time` times already.

**Names** (checked against `tests/std_names.py`'s rule that no two modules export one name; none of
`Expiring`, `expiring`, `expiring_for`, `left_ms` is exported today):

| Name | Signature |
|---|---|
| `Expiring[T]` | record: `expires_at: int`, `value_at: fn(int) -> result[T]` |
| `expiring` | `[T](value: T, expires_at: int) -> Expiring[T]` |
| `expiring_for` | `[T](value: T, now: int, ms: int) -> Expiring[T]` |
| `left_ms` | `[T](e: Expiring[T], now: int) -> int` |

The expiry moment is read as the field `e.expires_at`; no function for it (R2's reason).

## R5. A record built by hand

**Decision**: allowed, documented in the record's comment, as `tcp.Conn` documents it.
`new time.Expiring[str] { expires_at: 9, value_at: fn(now) => ok("x") }` is valid.

**Rationale**: it gives no access to anyone else's value: the program supplies its own function.
`new time.Expiring[str] { expires_at: 99999, value_at: t.value_at }` makes `expires_at` and `left_ms`
lie, while `value_at` still answers by the true moment, so the value stays guarded. Preventing this
needs private fields (R1).

## R6. Boundary, zero and negative durations, overflow

**Decision**: valid when `now < expires_at`. `expiring_for(v, now, ms)` is `expiring(v, now + ms)` with
no check on `ms`: 0 or less gives a value that is already expired. `now + ms` past the int range stops
with R0001 at that line (observed: `runtime error[R0001]: Integer overflow` with the trace through
`expiring_for`).

**Rationale**: "expired at or after the moment" is the rule of a JWT's `exp` and of the session expiry
`std.security` computes (`token_user(part, now_s)`); one rule in the library. Overflow is the
language's defined behavior and needs no code.

## R7. Recording and replay (FR-005, SC-003)

**Decision**: nothing to do. No function added here reads the clock or has an effect; `now_ms` is
recorded and replayed as today. `vo test` runs with the clock at 0, and tests of expiry pass times as
plain ints.

## R8. `std.security` (FR-011)

**Decision**: unchanged. Its session and token expiry are internal comparisons against a time it is
given; switching them to `Expiring` changes no behavior and is not asked for.

## R9. Documentation and tests

- `std/time.vo`: each new item gets its comment with an executed example line, in the file's style
  (`tests/doc_examples.py`); the module's first comment line gains "and values that expire".
- `docs/reference.md`: the `std.time` row of the library table gains
  ``; `Expiring[T]` (`expiring`, `expiring_for`, `left_ms`): a value read only with a time``.
  The growth in tokens is stated in the change.
- `site/docs/guide/standard-library.md`, `std.time` section: one executed example (fetch a token, reuse
  it until 60 s before it expires) and the store-and-rebuild pattern.
- `CHANGELOG.md`, `docs/roadmap.md`: one entry each.
- Golden cases in `tests/cases/time/` (new cases fail first, Principle IV):
  `expiring` (reads at the four times of story 1, `expiring_for`, `left_ms`, a nested value, the token
  function with `try`), `expiring_misuse` (the five reports of R1, plus sending one to a worker),
  `expiring_overflow` (R0001, exit 70), and one recorded run under `tests/cases/replay/`.
- SC-002 (an agent writes the token function from the documentation alone): five isolated runs with
  `claude -p`, no tools, an empty working directory and no user settings; run only when the user asks,
  as it spends their tokens.
