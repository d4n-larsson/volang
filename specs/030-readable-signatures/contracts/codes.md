# Contract: Catalog Codes (spec 030)

New codes, each with a `docs/errors.md` entry (example and fixed version), a `vo explain` text and golden
cases in `tests/cases/effect_sets/`. Messages and fixes: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0513 | `effect_set_cycle` | 65 | no | an effect set includes itself, directly or through other sets |
| W0505 | `unused_effect_set` | 0 | yes | a set that is not exported and that no clause of the program uses; the fix removes its declaration |

Changed:

| Code | Change |
|---|---|
| E0509 `unknown_effect` | a capitalized name that is neither an effect parameter nor an effect set in scope; the fix names the nearest effect, parameter or set |
