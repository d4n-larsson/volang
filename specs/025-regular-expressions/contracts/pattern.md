# Contract: pattern syntax and semantics

Patterns are vo strings, so a backslash is written `\\` (`"\\d+"` is the pattern `\d+`). Below, patterns
are shown as the engine sees them.

## Grammar

```text
pattern     = alternative { "|" alternative }
alternative = { piece }
piece       = atom [ quantifier ]
quantifier  = ( "*" | "+" | "?" | "{" n "}" | "{" n ",}" | "{" n "," m "}" ) [ "?" ]     n, m ≤ 1000, n ≤ m
atom        = literal | "." | class | shorthand | assertion | group
group       = "(" pattern ")" | "(?:" pattern ")" | "(?<" name ">" pattern ")"
            | "(?" flags ")" | "(?" flags ":" pattern ")"
flags       = one or more of "i" "m" "s" (a "-" before flags turns them off: "(?-i)")
class       = "[" [ "^" ] item { item } "]"       a "]" first is literal; "-" first or last is literal
item        = char | char "-" char | shorthand
shorthand   = "\d" | "\D" | "\w" | "\W" | "\s" | "\S"
assertion   = "^" | "$" | "\b" | "\B"
literal     = any code point except metacharacters, or "\" followed by a metacharacter
              (. ^ $ * + ? ( ) [ ] { } | \ / - ) or "\n" "\t" "\r"
name        = [A-Za-z_][A-Za-z0-9_]*  (unique in the pattern)
```

A `{` that does not start a valid quantifier is an error (no literal `{` without `\`), so `a{,3}` and
`x{` are reported instead of silently matching text.

## Semantics

| Rule | Meaning |
|---|---|
| search | every operation looks for the leftmost match anywhere in the text |
| preference | among matches at the same start, the first alternative and greedy quantifiers win (lazy `?` reverses), as Perl, Python and JavaScript |
| `.` | any code point except `\n`; with `(?s)` any code point |
| `^`, `$` | start and end of the text; with `(?m)` also after and before each `\n`; `$` never matches before a final `\n` without `(?m)` |
| `\b`, `\B` | ASCII word boundary (between `\w` and `\W` or the text's ends) and its negation |
| `\d \w \s` | `[0-9]`, `[A-Za-z0-9_]`, `[ \t\n\r\f\v]`; uppercase forms negate |
| `(?i)` | ASCII letters match both cases |
| code points | the text is UTF-8; one class or `.` matches one code point; offsets are bytes |
| groups | numbered by opening parenthesis, group 0 the whole match; inside a repetition, the last iteration, including a final empty one as in Python (`(a\|)*` on `aa` leaves group 1 empty) |
| empty matches | allowed anywhere, including right after a non-empty match; after an empty match the next search starts one code point later (Python 3.7+) |
| `split_on` | the parts between matches; an empty match at position 0 or at the end gives an empty part there, as Python's `re.split` |

## Not supported (E0320)

| Feature | Written | Message |
|---|---|---|
| backreference | `\1` … `\9`, `\k<name>` | "Backreferences are not supported: vo patterns run in linear time. Capture both parts and compare them in code." |
| lookahead | `(?=…)`, `(?!…)` | "Lookahead is not supported: vo patterns run in linear time. Match more and check the rest in code." |
| lookbehind | `(?<=…)`, `(?<!…)` | "Lookbehind is not supported: vo patterns run in linear time. Capture what comes before and check it in code." |
| atomic group, possessive | `(?>…)`, `*+ ++ ?+` | "Atomic groups and possessive quantifiers are not supported (and not needed: matching never backtracks)." |
| Unicode class | `\p{…}`, `\P{…}` | "Unicode classes are not supported; vo's `\w` and `\d` are ASCII. List the characters in a class." |
| other escapes | `\A \z \Z \G \x.. \u.... \e`, anchors and code escapes | "Unsupported escape `$1`: use `^`/`$`, or write the character itself." |
| Python named group | `(?P<name>…)` | "Named groups are written `(?<name>...)`." — fix `(?P<` → `(?<` |
| inline comment, `x` flag | `(?#…)`, `(?x)` | "Comments and the `x` flag are not supported." |
