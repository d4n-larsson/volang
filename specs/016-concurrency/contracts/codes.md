# Contract: Catalog Codes (spec 016)

Three new codes, each with a `docs/errors.md` entry, a `vo explain` text and a golden case.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0314 | `task_result_function` | 65 (check) | no | the result type of `concurrent_map`'s function contains a function type; message: "A task's result cannot hold a function (field `on_done` of `Job`): return data instead." (the place: a field or case, `list element`, `map value`, `collection element`, `` `ok` value ``, `the result itself`) |
| E0511 | `task_reads_input` | 65 (check) | no | the function passed to `concurrent_map` has the `input` effect; message: "Tasks cannot read stdin: read it before `concurrent_map` and pass the text in." |
| R0015 | `task_result_order` | 70 | — | a task's result holds a collection created with its own comparison function; message: "A task cannot return a collection with its own order: return its items (`keys`) instead." Also, with "A task's result cannot hold a function.", when a type parameter of the result stands for a function type |

Existing codes with new triggers (no new meaning):

| Code | New trigger |
|---|---|
| R0007 | `concurrent_map` with `limit` outside 1..64 |
| R0009 | the deadline passes in a task (re-raised by the parent) |
| any R code | raised in a task: re-raised by the parent with the task's code, message, span and trace |
| T0003 | the system refuses to create a task (`clone` or `pipe2` fails, e.g. a process limit): an internal error report, exit 125 |

E05xx placement: E0511 is an effect rule (what a function value may do where it is passed), next to
E0502 and E0505. E0508 is unassigned and stays unassigned (not reused for a different meaning).
