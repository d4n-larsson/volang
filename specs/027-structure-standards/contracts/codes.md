# Contract: Catalog Codes (spec 027)

New codes, each with a `docs/errors.md` entry (example and fixed version), a `vo explain` text and golden
cases in `tests/cases/structure/`. Messages, spans and related locations: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| W0701 | `effectful_logic` | 0 | no | a function with effects (not a test or property) has 15 or more statements that use no effect |
| W0702 | `file_too_long` | 0 | no | a program file has more than 400 code lines |
| W0703 | `function_too_long` | 0 | no | a function, test or property has more than 50 statements |
| W0704 | `duplicate_code` | 0 | no | 6 or more consecutive statements equal, up to local names and literals, to earlier ones in the program |
| W0705 | `test_elsewhere` | 0 | no | a test or property calls only program functions of one other file |
| W0706 | `unused_allow` | 0 | yes | an `@allow` that silences nothing; the fix removes its line |
| E0121 | `invalid_allow` | 65 | no | `@allow` without exactly two string arguments, with a code that is not W0701–W0705, or with an empty reason |

Changed:

| Code | Change |
|---|---|
| E0116 `unknown_annotation` | the closed set is `@spec` and `@allow`; the fix replaces a name within two letters of either |
| E0117 `misplaced_annotation` | `@allow` may stand above a `fn`, `test` or `prop`; `@spec` still only above a `test` or `prop` |

Structure warnings (W0701–W0706) are reported only by `vo check`, only for the program's own files, and
only when the program has no errors.
