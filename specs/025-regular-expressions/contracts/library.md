# Contract: `std.regex`

Module header: "std.regex: Regular expressions that run in linear time: test, find, replace and split
with compiled patterns." Import items one by one: `import std.regex.NAME;`. No effect is needed.

## Types

```vo
# A compiled pattern: make one with regex("...") or parse_regex(s), and reuse it.
export record Regex {
    pattern: str;
}

# One match: its text, byte offsets, groups (0 is the whole match) and named groups.
export record Match {
    text: str;
    start: int;
    end: int;
    groups: list[str];
    took_part: list[bool];
    named: map[str, str];
}
```

## Functions

| Function | Signature | Notes |
|---|---|---|
| `regex` | `(pattern: str) -> Regex` | `pattern` must be a string literal; checked by `vo check` (E0319–E0321) |
| `parse_regex` | `(pattern: str) -> result[Regex]` | for patterns from data; `err(message)` on a bad pattern |
| `is_match` | `(re: Regex, s: str) -> bool` | whether the pattern matches anywhere in `s` |
| `first_match` | `(re: Regex, s: str) -> result[Match]` | the leftmost match; `err("no match")` |
| `all_matches` | `(re: Regex, s: str) -> list[Match]` | every non-overlapping match, left to right |
| `replace_all` | `(re: Regex, s: str, replacement: str) -> str` | every match replaced; `$1`, `${1}`, `${name}`, `$$` |
| `split_on` | `(re: Regex, s: str) -> list[str]` | the parts between matches (`n + 1` parts for `n` matches) |
| `escape` | `(s: str) -> str` | `s` with every metacharacter escaped, for building a pattern from text |

## `vo doc` examples (each is checked by `tests/doc_conformance.py`)

```text
regex:        is_match(regex("^[a-z]+$"), "abc") is true
parse_regex:  parse_regex("a(") == err("at byte 1: The group is not closed: add `)`.") is true
is_match:     is_match(regex("\\d"), "a1") is true
first_match:  first_match(regex("(\\w+)@(\\w+)"), "mail al@vo now").groups[2] ... (via a match block)
all_matches:  len(all_matches(regex("\\d+"), "1 22 333")) is 3
replace_all:  replace_all(regex("(\\d+)-(\\d+)"), "10-20", "$2-$1") is "20-10"
split_on:     split_on(regex("\\s*,\\s*"), "a , b,c") == ["a", "b", "c"] is true
escape:       escape("1+1=2?") is "1\\+1=2\\?"
```

(Final wording of each example is fixed in `std/regex.vo`; `first_match`'s example uses a value that
`vo doc` can check, e.g. `first_match(regex("b+"), "abbc") == ...` with a full `Match`.)

## Reference text (budget)

Standard-library table row:

> | std.regex | `regex`, `parse_regex`, `is_match`, `first_match`, `all_matches`, `replace_all`, `split_on`: linear-time patterns; write `\\d` for `\d` |

One sentence in the strings bullet is not needed: the row says it. Estimated +40 tokens.
