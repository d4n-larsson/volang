# Contract: Annotations and `@spec`

```text
item       := annotation* (test | prop | fn | record | variant | import | export ...)
annotation := "@" NAME "(" STRING ("," STRING)* ")"
```

- The set of annotations is closed; it has one member, `spec`, allowed before `test` and `prop`.
- `@spec` IDs: `PART(/PART)*`, PART = `[A-Za-z0-9_-]+` (`FR-003`, `SC-002`, `US1/AC2`).
- Annotations change no checking or running of their item.
- `vo fmt`: each annotation on its own line directly above the item, `@spec("A", "B")`, IDs sorted.

## Reports

| Where | What |
|---|---|
| `vo test --json`, `test` record | `"spec":["FR-002","US1/AC1"]`, sorted, when annotated |
| `vo test`, a test that does not pass | the line ends with ` [FR-002, US1/AC1]` |
| `vo test` (annotated programs), just before the summary | text `spec FR-002: 2 tests, 1 not passing` (or `all passing`); JSON `{"v":1,"kind":"spec_result","id":"FR-002","tests":2,"passed":1}` |
| `vo query specs ENTRY` | `FR-002  main.vo:14 test "adds a note"`, one line per (ID, test), IDs sorted; JSON one `spec` record per ID |

## Diagnostics

See [codes.md](codes.md).
