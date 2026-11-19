# Contract: Catalog Codes (spec 025)

New codes, each with a `docs/errors.md` entry, a `vo explain` text and golden cases in
`tests/cases/regex/`. Messages, spans and fixes: [diagnostics.md](diagnostics.md). E0103's Fixable
changes from no to yes (its fix doubles the backslash).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0103 | `invalid_escape` | 65 (check) | yes | an unknown escape in a string; the fix doubles the backslash (`\d` → `\\d`) (changed: was no) |
| E0319 | `regex_pattern_invalid` | 65 (check) | no | a literal pattern of `regex` has a syntax error or exceeds a limit |
| E0320 | `regex_unsupported` | 65 (check) | sometimes | a literal pattern uses a feature that cannot run in linear time, or another unsupported form; the fix rewrites `(?P<` when it is the only problem |
| E0321 | `regex_not_literal` | 65 (check) | no | `regex` is called with something other than a string literal |
| E0322 | `regex_replacement_invalid` | 65 (check) | sometimes | a literal replacement of `replace_all` has a bad `$`, an unknown group, or Python's `\1`; the fix rewrites `\1` when it is the only problem |
