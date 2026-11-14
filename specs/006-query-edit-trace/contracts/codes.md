# Contract: New Catalog Codes

Each code gets an entry in `docs/errors.md` (name, summary, fixable or not, explanation, example, fix) and at least one golden case (`tests/catalog.py` enforces this). No existing code changes meaning.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| T0004 | `unknown_target` | 64 | no | A query or edit target that names nothing in the program. The message gives the closest name when there is one. |
| T0005 | `ambiguous_target` | 64 | no | `name` matching items in several files. `related` lists them; use `FILE:LINE:COL`. |
| T0006 | `edit_refused` | 65 | no | An edit whose result would not check, or would change what a reference means; a bad new name or field; a program whose errors the edit would change; files that cannot be written. Nothing is written. |
| T0007 | `diff_mismatch` | 64 | no | `vo impact --diff` with a diff that cannot be read as a unified diff, or whose lines do not match the current files. |
| T0008 | `invalid_recording` | 64 | no | `vo replay` of a file that is not a complete recording (bad JSON, unknown kind, missing header or `end`). |
| T0009 | `cannot_write_output` | 74 | no | A `--trace` or `--record` file that cannot be created, or whose writes fail. Reported once, at exit; the exit status becomes 74 unless the run already had a non-zero status. |
| R0014 | `replay_mismatch` | 70 | — | During `vo replay`, the program asks for an input that is not the next recorded one. |

Existing codes reused without a change in meaning:

| Code | New situations |
|---|---|
| T0001 `usage_error` | Missing query arguments; `--trace-limit` without `--trace`; `--trace`/`--record` on a source file; `--seed`/`--clock-ms`/`--` with `replay`; `--dry-run` outside `edit`. |
| T0002 `cannot_read_input` | Unreadable diff or recording (a trace or recording that cannot be *written* is T0009). |
| R0010 `capability_denied` | Replayed from a `denied` event. |
