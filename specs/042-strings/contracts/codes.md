# Contract: codes

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| W0102 | `raw_string_not_needed` | 0 | yes | A raw string whose text has no backslash, no quote and no line break. |
| E0103 | `invalid_escape` | 65 | sometimes | An escape other than the six, or a malformed `\u{HEX}` (changed here: not every case has a fix). |
