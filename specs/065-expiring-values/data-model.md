# Data Model: Expiring values

One record, added to `std/time.vo`. No toolchain data structure changes.

## `time.Expiring[T]`

| Field | Type | Meaning |
|---|---|---|
| `expires_at` | `int` | Milliseconds since 1970-01-01T00:00:00Z. The first moment at which the value is no longer valid. |
| `value_at` | `fn(int) -> result[T]` | Given a time: `ok(value)` when the time is before `expires_at`, otherwise `err("expired at E, read at N")`. Holds the value; has no effects. |

**Rules**

- Valid at `now` exactly when `now < expires_at`.
- Not changed after it is made. A fresh token is a new `Expiring`.
- Made by `expiring` or `expiring_for`. One built by hand with `new` is valid vo; its `value_at` is
  then whatever function the program gave (research R5).
- Because it holds a function: no `==` (E0308), no JSON (E0328), no `str` or `format` (E0301), and it
  cannot be sent to a worker. A record that holds one inherits all four.

**States**: none stored. "Valid" and "expired" are answers to a read at a time, not a field; nothing
happens when the moment passes.

## The error text

`expired at <expires_at>, read at <now>`: both as decimal ints, no units, no calendar form. Fixed;
golden cases compare it.

## Stored form (User Story 3)

Not a type. A program that keeps an expiring value in a database or sends it elsewhere keeps two
ordinary fields, the value and `expires_at`, and calls `expiring(value, expires_at)` on the way back.
