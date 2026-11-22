# Contract: codes

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| E0105 | `number_out_of_range` | 65 | sometimes | As before, and: `0x` or `0b` without digits, more than 16 hex or 64 binary digits, an upper-case prefix (changed here: that last case has a fix). |
| E0101 | `invalid_character` | 65 | sometimes | As before (changed here: `&`, a bar, `^`, `~`, `<<` and `>>` name `std.bits` and have no fix; `?` never had one). |
