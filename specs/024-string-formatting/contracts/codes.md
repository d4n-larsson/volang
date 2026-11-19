# Contract: Catalog Codes (spec 024)

New codes, each with a `docs/errors.md` entry, a `vo explain` text and golden cases in
`tests/cases/format/`. Messages, spans and fixes: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0120 | `f_string` | 65 (check) | sometimes | a string literal written with an `f` prefix; the fix writes the `format` call when every hole is a name or field access |
| E0315 | `format_template_not_literal` | 65 (check) | no | the first argument of `format` is not a string literal |
| E0316 | `format_argument_count` | 65 (check) | sometimes | placeholders and arguments differ in number; the fix translates C `%` verbs |
| E0317 | `format_placeholder_invalid` | 65 (check) | sometimes | a lone brace, an index or name, an unsupported or out-of-order option, a bound, a forbidden combination |
| E0318 | `format_option_type` | 65 (check) | sometimes | an option that does not apply to the argument's type; the fix converts an `int` with `float(...)` for `.Nf` |
