# Glob match

Match file paths against glob patterns.

## Input

UTF-8 text in lines (each ends with `\n`; the last line may lack it). First come the patterns, one per line, numbered from 1; then a line that is exactly `%%`; then the paths, one per line. There may be no patterns or no paths. Every pattern and path line is non-empty.

A path is one or more segments separated by single `/` characters: no `/` at the start or end, no empty segments. A segment can contain any characters but `/` (spaces, `*`, `[`, `\`, non-ASCII letters, ...).

A pattern is likewise split at every `/` into pattern segments; no pattern starts or ends with `/` or contains `//`. A pattern has at most 100 characters, a path at most 200.

## Matching

A pattern matches a path when its segments, from left to right, match all of the path's segments. Characters are Unicode code points, and matching is case-sensitive.

A pattern segment that is exactly `**` matches zero or more whole path segments: `a/**` matches `a`, `a/b` and `a/b/c`; `**/b` matches `b` and `x/y/b`. Any other pattern segment matches exactly one path segment, read from left to right:

- `?` matches any one character.
- `*` matches any run of characters, also an empty one (`**` inside a longer segment, as in `a**`, is just two `*`).
- `\` followed by a character matches that character exactly (`\*` matches `*`). A `\` that is the last character of a pattern segment matches a `\`.
- `[` may start a character class, read from left to right. A `!` right after the `[` negates the class. Then come members up to the closing `]`: a member is `\` followed by any character (that character), or any character other than `\` and `]`; a `]` right after the `[` (or after `[!`) is also a member. If a member X is followed by `-` and then by another member Y, the three form a range: every character whose code point is from X to Y (none if X > Y), and reading goes on after Y. Any other `-` is an ordinary member. The class matches one character that is (or, if negated, is not) a member or in a range. If the segment ends before the closing `]`, the `[` is not a class but an ordinary character, and matching goes on with the character after it.
- Every other character matches itself.

Hidden segments: a path segment that starts with `.` is matched only by a pattern segment that starts with a `.` matching that first `.` as an ordinary character (`.` or `\.`). It is never matched by `**`, and `?`, `*` and classes never match its first character: `*` does not match `.git`, `.*` and `\.g*` do; `**` does not match `a/.git/b` but `**/.git/**` does.

## Output

One line per path, in input order: `PATH -> LIST`, where LIST is the numbers of the matching patterns in increasing order separated by `,`, or `none`. Every output line ends with a newline.
