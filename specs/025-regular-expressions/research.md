# Research: Regular Expressions

## R1. Engine: a Pike VM written for vo

**Decision**: `src/regex.c` compiles a pattern to a program of a few instruction kinds (`CHAR c`,
`CLASS ranges`, `ANY`/`ANY_NL`, `SPLIT x y` (x preferred), `JMP x`, `SAVE n`, `ASSERT kind` for `^ $ \b
\B` and their multiline forms, `MATCH`) and runs it with a Pike VM: all threads advance together one
code point at a time, a thread list is ordered by priority, and a state already in the list is not added
again. Each thread carries its capture array (copied on `SAVE`). The first thread to reach `MATCH` cuts
the lower-priority threads, which gives leftmost-first semantics with Perl's greedy/lazy preferences
(Russ Cox, "Regular Expression Matching: the Virtual Machine Approach"; the design of RE2's NFA).

Time: O(len(text) × len(program)); memory: O(len(program) × groups). No recursion over the text.

**Rationale**: linear time for every pattern (the spec's safety requirement); the same submatch
semantics as Python for the supported subset (the differential test's oracle); small enough to own and
to port to three platforms without assembly. Vendoring was ruled out: RE2 is C++ and Rust's regex is
Rust (neither builds freestanding C), and the C libraries (PCRE2, Oniguruma, TRE) backtrack or are
large and use libc.

**Alternatives considered**: a DFA (faster, but submatches need a second pass, and state explosion needs
a cache with eviction: RE2's complexity); a backtracking matcher with a step limit (an arbitrary limit is
a nondeterministic-looking failure for agents; rejected by the spec).

## R2. Speed: prefilter and capture-free test

**Decision**:
- **First-byte set**: at compile time, the set of bytes a match can start with (all bytes when the
  program can match empty or starts with an assertion other than `^`). An unanchored search skips
  positions whose byte is not in the set when no thread is alive, instead of starting a thread at every
  position.
- **Anchored start**: a pattern starting with `^` (not `(?m)`) tries position 0 only.
- **Test without captures**: `is_match` runs the VM with no capture copying and stops at the first
  `MATCH`.
- **Budget**: the match loop spends one work unit per 64 code points × program length / 64 against
  `rt_budget` and calls `rt_poll` (the existing `--timeout` mechanism), so a long match stops at the
  deadline (FR-013).

**Rationale**: SC-003 (10 MB of logs in under 2 s for a date-and-level pattern): most positions of a log
line fail on the first byte. Measured in `tests/regex_speed.py`; ponytail: a lazy DFA if a measured
target fails.

## R3. Compile cache keyed by the pattern text

**Decision**: a `Regex` record holds only its pattern text. The primitives compile on first use and keep
the program in a cache of 16 slots keyed by the pattern bytes (round-robin replacement; each slot's
buffers are reused, so memory is bounded by 16 × the program size limit). A program compiled once is
reused for every match of the same pattern.

**Rationale**: no opaque type is needed in the language (a record with a `str` field suffices), nothing
compiled crosses into the collected heap, and a forged `Regex { pattern: "(" }` is just a pattern that
fails to compile (R0007 at the first use). Loops that reuse one `Regex` compile once.

**Alternatives considered**: storing a compiled program as `bytes` in the record (the matcher would have
to validate untrusted program bytes on every call); a new builtin type `regex` (a type keyword and
reference text for one library).

## R4. Primitives and their encodings

**Decision**: two primitives (std-only, `__` names, at most three arguments as every builtin):

| Primitive | Signature | Returns |
|---|---|---|
| `__re_compile(pattern)` | `s>r` | `ok(names)`: the capture group names in group order, `\n`-separated, `""` for unnamed groups (so the count is lines + 1 with group 0 implied); `err(message)` with the same message the checker gives |
| `__re(op, pattern, text)` | `iss>I` | op 1: `[]` or `[1]` (test); op 2: the first match, `[s0, e0, s1, e1, ...]` with `-1, -1` for a group that did not take part, or `[]`; op 3: every match, the same records one after another (the group count is known from `__re_compile`) |

Offsets are bytes. A bad pattern in `__re` is R0007 "Invalid pattern: MESSAGE." at the user's call
(`std_user_call`).

The vo side (`std/regex.vo`) builds `Match` records with `slice`, expands replacements (`$n`, `${n}`,
`${name}`, `$$`) and splits, over these offsets.

**Rationale**: no records constructed in C; the same encoding style as `__run` and `__fs`.

## R5. Check-time patterns (`src/check_regex.c`)

**Decision**: in `check_call`, when the callee is a function of the standard library file `regex.vo`:
- `regex(arg)`: `arg` must be `N_STR` (else E0321 "`regex` needs a string literal; use `parse_regex(s)`
  for a pattern built at run time."). The literal is decoded with `lit_decode` (shared with
  `check_format.c`, giving the source offset of every decoded byte), compiled by the engine's parser, and
  each error becomes E0319 or E0320 at the source span of the pattern bytes it names. The parser stops
  at the first error of a pattern (one diagnostic per pattern; others are usually cascades).
- `replace_all(re, s, repl)` with `repl` a literal: its references are checked (E0322); when `re` is a
  direct `regex("…")` call or a `let` local whose initializer (`sym->decl`, an `N_LET`) is one, group
  numbers and names are checked against that pattern. A `\1`…`\9` or `\g<name>` gets the fix to `$1` /
  `${name}`.

A literal that already has a lexer error (E0103) is not checked again.

**Rationale**: Principle II; the cases agents hit first are all literal.

## R6. E0103's fix

**Decision**: the lexer's E0103 gets one fix per unknown escape: insert a second backslash
(`\d` → `\\d`). It is always the reading that makes the string valid and keeps its characters, so it is
unambiguous; the catalog's Fixable becomes `yes`.

## R7. Semantics decisions checked against Python

- `$` without `(?m)`: end of text only (Python's `\Z`); with `(?m)`: before every `\n` and at the end.
- `\d \w \s \b`, `(?i)`: ASCII (Python with `re.ASCII`).
- `.` without `(?s)`: any code point except `\n`.
- Empty matches in `all_matches`, `replace_all`, `split_on`: Python 3.7+ rules (an empty match is
  allowed right after a non-empty one; the next search starts one code point further after an empty
  match).
- Captures inside repetitions: the last iteration's text, as Python. Known risk: Pike VMs and
  backtracking engines can differ on captures of groups that match empty inside a star (`(a?)*`). The
  differential test (R8) decides: where the VM differs, the VM is changed to Python's result if that
  stays linear (RE2 does this for empty-width loops); otherwise the construct is excluded from the
  generator and listed in `results.md`.

## R8. The differential test

**Decision**: `tests/regex_diff.py` generates patterns from a grammar of the supported syntax (literals
from a small alphabet `a b c` plus `.`, `-`, digits and spaces so matches are frequent; classes,
shorthands, groups (capturing, named, non-capturing), alternation, all quantifiers greedy and lazy with
small counts, anchors, flags) and texts from the same alphabet (plus `é` and `\n`), 0–40 code points.
For each pair it compares `all_matches` (offsets and groups, converted from Python's code point offsets
to bytes), `replace_all` with a template that uses every group, and `split_on`, with Python's
`re.finditer`, `re.sub` and `re.split` under `re.ASCII` and the `$` → `\Z` translation. vo programs of
2,000 calls each; seeded; `--quick` 5,000 pairs; `--cross BIN…` for the three platforms.

## R9. Limits

| Limit | Value | Reported as |
|---|---|---|
| pattern length | 10,000 bytes | E0319 / `err` "Pattern is longer than 10000 bytes." |
| repetition count | 1,000 (`{n}`, `{n,m}`) | E0319 "Repetition count above 1000." |
| program size after expanding repetitions | 20,000 instructions | E0319 "Pattern is too large (more than 20000 instructions after repetitions)." |
| nesting depth of groups | 200 | E0319 "Groups nested more than 200 deep." |
| capture groups | 1,000 | E0319 |

Bounded memory per match: two thread lists of the program's size, captures of `2 × groups` ints per
thread.
