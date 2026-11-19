# Contract: regex diagnostics

Spans are byte ranges in the source; for a pattern or replacement literal, the range covers the source
bytes of the part concerned (escapes included: `\\d` in the source is the pattern's `\d`).

## E0103 (existing) `invalid_escape`: new fix

For each unknown escape `\X` in a string, the fix inserts a second backslash: `"\d+"` → `"\\d+"`.
Fixable: yes (was no).

## E0319 `regex_pattern_invalid`

- **When**: a literal pattern of `regex(...)` has a syntax error or exceeds a limit (pattern.md, research
  R9). The first error of a pattern only.
- **Messages** (examples): "Missing `)`: the group opened here is not closed.", "Unmatched `)`.",
  "Missing `]`: the class opened here is not closed.", "`*` has nothing to repeat.", "Range `z-a` is
  reversed.", "Repetition `{3,1}` has its minimum above its maximum.", "Repetition count above 1000.",
  "`{` must start a repetition such as `{2}` or `{2,5}`; write `\\{` for the character.", "Group name
  `$1` is used twice.", "Pattern is too large (more than 20000 instructions after repetitions)."
- **Fix**: none.

## E0320 `regex_unsupported`

- **When**: a literal pattern uses a feature in pattern.md's "Not supported" table.
- **Message**: that table's message.
- **Fix**: only `(?P<` → `(?<`.

## E0321 `regex_not_literal`

- **When**: `regex(x)` where `x` is not a string literal.
- **Span**: the argument.
- **Message**: "`regex` needs a string literal, so the pattern can be checked; use `parse_regex(s)` for
  a pattern built at run time."
- **Fix**: none (`parse_regex` returns a `result`, which changes the code around the call).

## E0322 `regex_replacement_invalid`

- **When**: a literal replacement of `replace_all` has a `$` not followed by digits, `{digits}`,
  `{name}` or `$`; or refers to a group the pattern does not have (when the pattern is known, research
  R5); or uses Python's `\1`…`\9` (in the source `\\1`) or `\g<name>`.
- **Messages**: "`$` must be followed by a group number, `{name}` or `$` (write `$$` for a dollar sign).",
  "The pattern has no group $1.", "Group references are written `$1` or `${name}`."
- **Fix**: `\\1` → `$1`, `\\g<name>` → `${name}`; none otherwise.

## Run time

| Situation | Report |
|---|---|
| `parse_regex` of a bad pattern | `err("at byte N: MESSAGE")`, MESSAGE the E0319/E0320 message |
| a `Regex` built by hand with a bad pattern, at its first use | R0007 "Invalid pattern at byte N: MESSAGE" at the user's call |
| a run-time replacement with a bad `$` or an unknown group | R0007 with the E0322 message |
| `--timeout` during a match | R0009 at the call |
