# Data Model: Documentation on hover

## Doc record (the `vo doc --json` record; nested as `doc` in `type-at`)

| Field | fn | record | variant | field (new) | case (new) | builtin |
|---|---|---|---|---|---|---|
| `item` | `fn` | `record` | `variant` | `field` | `case` | `builtin` |
| `module` | file module | file module | file module | file module | file module | `builtins` |
| `name` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| `owner` | | | | record name | variant name | |
| `signature` | `fn f[T](a: A) -> R uses E` | `record R { ... }` | `variant V { ... }` | `x: float` | `circle(float)` | the table signature |
| `tparams`, `params`, `returns`, `effects` | ✓ | | | | | |
| `fields` / `cases` | | ✓ | ✓ | | | |
| `description` | when commented | when commented | when commented | when commented | when commented | the table notes |
| `example` | when present | when present | when present | | | |

**Module** of a program file: its import path relative to the entry's directory (`geo`, `util.text`,
`main`); `std/x.vo` is `std.x`.

## Doc comment

The `#` lines directly above an item or member, with no blank line between: description lines are joined
by spaces; the topmost `#     ` line (five spaces) is the example.
