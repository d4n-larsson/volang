# Contract: Catalog Codes (spec 034)

New code, with a `docs/errors.md` entry, a `vo explain` text and a golden case in
`tests/cases/build/damaged`.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| T0014 | `damaged_executable` | 66 | no | a built executable whose program part is cut short or does not match its hash |

Reused: T0001 `usage_error` (bad `--base`, bad OUTPUT, unknown `--vo-` option), T0009
`cannot_write_output` (OUTPUT not writable), E0504 (the effects record disagrees with the program).
