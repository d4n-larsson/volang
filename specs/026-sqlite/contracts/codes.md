# Contract: Catalog Codes (spec 026)

New codes, each with a `docs/errors.md` entry, a `vo explain` text and golden cases in
`tests/cases/sqlite/`. Messages, spans and fixes: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0323 | `sql_invalid` | 65 (check) | no | the literal of `sql` has a syntax error, more than one statement, a clock or random function, or a refused pragma |
| E0324 | `sql_not_literal` | 65 (check) | no | `sql` is called with something other than a string literal |
| E0325 | `sql_parameters` | 65 (check) | sometimes | named or numbered parameters (fix to `?` when each is used once), or a parameter list of the wrong length |
| E0326 | `sqlite_memory_path` | 65 (check) | yes | `open(":memory:")` or `open_readonly(":memory:")`; the fix uses `open_memory()` |
| E0327 | `effect_argument_syntax` | 65 (check) | sometimes | effects written as type arguments without `uses`, `uses` not last, or effect arguments for a type without an effect parameter; the fix adds `uses` |
