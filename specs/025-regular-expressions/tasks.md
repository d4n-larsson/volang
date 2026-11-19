---

description: "Task list for vo-lang spec 025: Regular Expressions"
---

# Tasks: Regular Expressions

**Input**: Design documents from `specs/025-regular-expressions/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/library.md,
contracts/pattern.md, contracts/diagnostics.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup.
- Foundational: the engine (parser, compiler, Pike VM, cache), the two primitives, and `std/regex.vo`
  with `Regex`, `regex`, `parse_regex` and `is_match`, which every story needs.
- One phase per user story: US1 and US2 are P1, US3 and US4 are P2, US5 is P3.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/regex/`. The directory's default command is `run {}`
  (`tests/run.sh`); check cases add a `.cmd` with `check --json {}`, expect JSONL in `.out` and `65` in
  `.code` (as `tests/cases/format/count.*`).
- **Expected outputs** of run cases are computed with Python's `re` (`re.ASCII`, `$` written `\Z` when
  not multiline) before they are written into `.out`; byte offsets from Python's code point offsets.
- **Patterns in vo source** double their backslashes: the pattern `\d+` is written `"\\d+"`.
- **Diagnostics inside a literal** use `diag_add(code, SEV_ERROR, file, start, end, msg)` with source
  offsets from `lit_decode` (T006).
- **Between edits**: `make quick AREA=regex`. `make test` once per platform at the end (T047); never two
  suites at once.
- **Shortcuts**: marked `ponytail:` (the cache's round-robin replacement, no lazy DFA).

---

## Phase 1: Setup

- [X] T001 [P] Create `specs/025-regular-expressions/results.md` with sections SC-001 to SC-006,
  Verification, Size, Deviations, as `specs/024-string-formatting/results.md`.
- [X] T002 [P] `tools/quick.sh`: an area `regex` with cases `regex lex doc` and scripts `apply_fixes.py`,
  `catalog.py`, `doc_conformance.py`, `"regex_diff.py --quick"`; map `src/regex.c`,
  `src/check_regex.c`, `std/regex.vo`, `tests/regex_*.py` to it, placed before the `src/check*.c` line.
  Check with `QUICK_FILES=src/regex.c tools/quick.sh --which`.
- [X] T003 [P] `tests/apply_fixes.py`: add `regex` to the directories whose fixes are applied (the list
  that already holds `format`). `tests/catalog.py`: add `"025-regular-expressions"` to the specs whose
  `contracts/codes.md` it reads (after `"024-string-formatting"`).

---

## Phase 2: Foundational (blocks every story)

- [X] T004 [P] Golden run case `tests/cases/regex/basic`, failing until T011: `is_match` of
  `^[A-Z]{3}-\d{4}$` against `ABC-1234`, `ABC-12345`, `abc-1234` (prints `true false false`);
  `is_match(regex("a|b"), "xbx")` true; `is_match(regex("colou?r"), "color")` true;
  `is_match(regex("^$"), "")` true; `is_match(regex("é."), "aéz")` true; `parse_regex("(")` is an
  `err` (print the message); `parse_regex("a+")` is `ok`.
- [X] T005 `src/regex.c` (new): the parser and compiler (research R1, contracts/pattern.md). Pattern
  bytes in, a program out (data-model.md "Program"), or an error `{ start, end, code, message, fix }`
  ("`start`, `end`: byte range in the pattern text; `code`: E0319 (syntax or limit) or E0320
  (unsupported feature)"). In this task: literals and metacharacter escapes (`\n \t \r` too), `.`,
  classes with ranges, negation and the shorthands, `\d \D \w \W \s \S`, `^ $ \b \B` (non-multiline),
  groups `( )`, `(?: )`, `(?<name> )`, `|`, quantifiers `* + ? {n} {n,} {n,m}` and their lazy forms,
  every E0319 syntax error of contracts/diagnostics.md, and every E0320 feature of contracts/pattern.md
  "Not supported" (with the `(?P<` fix). Flags come in US5 (until then `(?i)` etc. are E0319 "Unknown
  group syntax"). Recursive descent with a nesting counter (limit 200, research R9); no recursion over
  the text. UTF-8 code points throughout (patterns are valid UTF-8: they are vo strings).
- [X] T006 `src/check_format.c`: rename its static `decode` into an exported
  `long lit_decode(Node *lit, char **buf, long **offs)` (returns the decoded length, or -1 when the
  literal is not a plain one-line string; `offs[n]` is the closing quote's offset), declared in
  `src/vo.h`; `check_format_call` uses it. `make quick AREA=format` stays green.
- [X] T007 `src/regex.c`: the Pike VM (research R1): two thread lists sized to the program, a sparse set
  per list so a state is added once, threads in priority order, `SPLIT x y` adds x before y, captures
  copied on `SAVE`, the first `MATCH` cuts lower-priority threads. Entry points:
  `long re_exec(Prog *p, char *s, long n, long from, long *caps, int want_caps)` (returns 1 and fills
  `caps[0..2g+1]` with byte offsets or -1, or 0) searching from byte `from`. Steps one code point at a
  time; assertions read the previous and next code points.
- [X] T008 `src/regex.c`: the compile cache (research R3): 16 slots keyed by pattern bytes, round-robin
  replacement, each slot's buffers reused (`ponytail:` comment: round-robin, an LRU if measurements show
  thrashing). `Prog *re_get(Str *pattern, Str **err)`.
- [X] T009 Primitives (research R4, data-model "Primitive encodings"):
  - ids 84 `__re_compile` (`"s>r"`) and 85 `__re` (`"iss>I"`) in `src/check_expr.c` (`builtin_names`,
    `builtin_sigs`, `B_COUNT` 85), `src/vo.h` (`B_RE_COMPILE`, `B_RE`, `B_LAST` 85),
    `src/rt_call.c` (enum, `_Static_assert`, dispatch), `src/check_effects.c` (pure);
  - `__re_compile(p)`: `ok` with the group names for groups 1..n joined by `\n` (`""` for unnamed), or
    `err(message)` where the message ends with " (at byte N)";
  - `__re(op, p, s)`: op 1 `[]`/`[1]`, op 2 the first match `[s0, e0, …]` with `-1, -1` for groups that
    did not take part, op 3 every match concatenated (empty-match rules of contracts/pattern.md); a bad
    pattern is R0007 "Invalid pattern: MESSAGE." at `std_user_call(call)`.
- [X] T010 `std/regex.vo` (new; module header and doc comments per contracts/library.md):
  `export record Regex { pattern: str; }`; `regex(pattern: str) -> Regex` (returns the record; the
  check-time check is US2); `parse_regex(pattern: str) -> result[Regex]` (`try __re_compile(pattern)`
  first); `is_match(re: Regex, s: str) -> bool` (`len(__re(1, re.pattern, s)) > 0`). Each with a
  `vo doc` example that `tests/doc_conformance.py` runs.
- [X] T011 Make T004 pass; `make quick AREA=regex`.

**Checkpoint**: T004 passes; `vo doc std.regex` lists the four items; doc examples run.

---

## Phase 3: User Story 1 — Validate and extract (P1) 🎯 MVP

**Goal**: the first match with its groups, offsets and named groups.

**Independent Test**: the run cases below, byte for byte.

- [X] T012 [P] [US1] Golden run cases in `tests/cases/regex/`, failing until T013:
  - `extract`: the log line of Story 1 with `(\d{4})-(\d{2})-(\d{2}) \S+ (?<level>ERROR|WARN)`: prints
    groups 0–4, `start`, `end`, `slice(line, m.start, m.end) == m.text`, and `m.named["level"]`;
  - `optional_group`: `(a)?b` on `b`: `groups[1]` is `""`, `took_part[1]` is false; on `ab`: true;
  - `no_match`: `first_match` on a line with no match is `err("no match")`, handled with `match`;
  - `utf8_offsets`: a match after `é` and `日本` has byte offsets (Python's code point offsets converted);
  - `leftmost_first`: `a|ab` on `ab` is `a`; `a*?` on `aaa` is `""`; `(a+)(a*)` on `aaa` groups `aaa`/`""`.
- [X] T013 [US1] `std/regex.vo`: `export record Match { text: str; start: int; end: int; groups:
  list[str]; took_part: list[bool]; named: map[str, str]; }` (field docs per data-model.md:
  "`groups`: group 0 the whole match, then groups 1..n by opening parenthesis; `""` for a group that
  did not take part"; "`named`: every named group's text by name (`""` when it did not take part)");
  `first_match(re: Regex, s: str) -> result[Match]` from `__re(2, …)` and the names of
  `__re_compile`; a private helper builds a `Match` from an offset record (shared with US3). Doc
  example checked.

**Checkpoint**: T012 passes.

---

## Phase 4: User Story 2 — Pattern mistakes are caught before the program runs (P1)

**Goal**: E0103's fix; E0319–E0321 at check time with spans inside the literal.

**Independent Test**: the check cases below; `tests/apply_fixes.py` re-checks every fix.

- [X] T014 [P] [US2] Golden check cases in `tests/cases/lex/` and `tests/cases/regex/`
  (`.cmd` `check --json {}`), messages verbatim from contracts/diagnostics.md, failing until T017–T020:
  - `lex/escape_fix` (or the existing E0103 case updated): `"\d+\s*\."` gives three E0103, each with
    the fix doubling its backslash;
  - `regex/syntax`: `regex("(a")`, `regex("a)")`, `regex("[abc")`, `regex("*a")`, `regex("[z-a]")`,
    `regex("a{3,1}")`, `regex("a{1001}")`, `regex("x{")`, `regex("(?<n>a)(?<n>b)")`: E0319 each, the span
    on the offending pattern bytes (escapes counted in the source);
  - `regex/unsupported`: `(?=x)`, `(?!x)`, `(?<=x)`, `(?<!x)`, `\\1`, `\\k<n>`, `(?>a)`, `a*+`,
    `\\p{L}`, `\\A`, `(?#c)`: E0320 with the table's message; `(?P<y>a)`: E0320 with the fix `(?<`;
  - `regex/not_literal`: `let p = "a"; regex(p)`: E0321 at `p`;
  - `regex/one_pass`: four bad patterns and an unrelated type error in one file, all reported.
- [X] T015 [P] [US2] `docs/errors.md`: entries E0319 `regex_pattern_invalid`, E0320
  `regex_unsupported`, E0321 `regex_not_literal`, E0322 `regex_replacement_invalid` after E0318, each
  with a paragraph, `Fixable:` (no / sometimes / no / sometimes), Example and Fixed; E0103's `Fixable:`
  becomes `yes` and its paragraph mentions the fix. `python3 tests/catalog.py` passes (after T020's
  cases exist).
- [X] T016 [P] [US2] Golden run case `tests/cases/regex/parse_errors`: `parse_regex` of each bad pattern
  of T014 prints the same message (with " (at byte N)") as its `err`; a hand-built
  `new Regex { pattern: "(" }` passed to `is_match` stops with R0007 "Invalid pattern: …" at the call
  (exit 70).
- [X] T017 [US2] `src/lex.c`: E0103 adds the fix inserting `\` before the escape's backslash
  (`diag_add_fix(d, file, pos, pos, "\\")`).
- [X] T018 [US2] `src/check_regex.c` (new): `void check_regex_call(Node *call, Node *decl)`, called from
  `check_call` in `src/check_expr.c` after a user-function call is checked, when `decl` is in a standard
  library file whose path ends `regex.vo` (`file_get(decl->file)->std`). For `regex`: a non-`N_STR`
  argument is E0321 at the argument; an `N_STR` literal is decoded with `lit_decode` (skipped when the
  literal has a lexer error: any E0103 in its span) and compiled with the engine's parser; an error
  becomes its code at `offs[start]..offs[end]` with its message and, when present, its fix at the same
  mapped range.
- [X] T019 [US2] Wire the check in `src/check_expr.c` (`check_call`'s `SYM_FN` branch) and declare
  `check_regex_call` in `src/vo.h`.
- [X] T020 [US2] Make T014 and T016 pass; `python3 tests/apply_fixes.py` resolves the E0103 and
  `(?P<` fixes.

**Checkpoint**: T014–T016 pass; `tests/catalog.py` clean.

---

## Phase 5: User Story 3 — Search, replace and split (P2)

**Goal**: `all_matches`, `replace_all`, `split_on`, `escape`, with Python's results, and E0322.

**Independent Test**: the run cases below and `tests/regex_diff.py` (SC-001).

- [X] T021 [P] [US3] Golden run cases in `tests/cases/regex/`, failing until T024–T026:
  - `replace`: `\s+` → `" "` on `"a  b\t\tc"`; `(\d{2})/(\d{2})/(\d{4})` with `$3-$2-$1`; `$$`;
    `${1}0`; `${name}`; card masking without lookahead: `^\d{12}(\d{4})$` → `************$1`;
  - `all`: `#\w+` on `"#vo is #fast, #vo"` (texts and offsets); `\d+` counts;
  - `split`: `\s*,\s*` on `"a , b,c"`; a split with no match (one part); with matches at both ends;
  - `empty_matches`: `x*` replaced by `-` on `"abxd"` → `-a-b--d-`; `all_matches(regex(""), "aé")`
    has 3 matches at byte offsets 0, 1, 3; `split_on(regex("x*"), "axb")` equals Python's
    `re.split` result;
  - `escape`: `escape("1+1=2?")` is `1\+1=2\?` and `is_match(try parse_regex(escape(s)), s)` for a text
    full of metacharacters;
  - `replace_runtime_error`: a replacement built at run time with `$9` on a one-group pattern stops with
    R0007 "The pattern has no group 9." (exit 70).
- [X] T022 [P] [US3] Golden check case `tests/cases/regex/replacement` (`.cmd` `check --json {}`):
  `replace_all(regex("(a)"), s, "\\1")` → E0322 with fix `$1`; `"\\g<x>"` with a named group `x` → fix
  `${x}`; `"$2"` on a one-group pattern (direct call, and through `let re = regex("(a)");`) → E0322 "The
  pattern has no group 2."; `"$x"` → E0322 about `$`; `"${nope}"` → no such group.
- [X] T023 [P] [US3] `tests/regex_diff.py` (research R8): generator of patterns over the supported
  syntax and texts; comparisons of `all_matches` (offsets in bytes and every group), `replace_all` with
  a template using every group, and `split_on` against Python's `re.finditer`, `re.sub`, `re.split`
  (`re.ASCII`; `$` → `\Z` outside `(?m)`); vo programs of 2,000 calls; `random.Random(1)`; 100,000
  pairs by default, `--quick` 5,000, `--cross BIN…`; prints the first difference with pattern and
  text; exit 1 on any difference.
- [X] T024 [US3] `std/regex.vo`: `all_matches` (from `__re(3, …)`, the group count from
  `__re_compile`), `replace_all` (parses the replacement once into literal and group pieces, R0007
  messages of data-model.md "Replacement"), `split_on` (parts between the matches of `all_matches`),
  `escape` (backslash before each of `. ^ $ * + ? ( ) [ ] { } | \`). Doc examples checked.
- [X] T025 [US3] `src/check_regex.c`: `replace_all` with a literal third argument: the replacement
  checks of contracts/diagnostics.md E0322; the group count and names come from the first argument
  when it is a direct `regex("…")` call or an `N_IDENT` whose `SYM_LOCAL` symbol's `decl` is an `N_LET`
  (not `var`) initialized with one.
- [X] T026 [US3] Run `tests/regex_diff.py`; for every difference, fix the engine toward Python's result
  when that stays linear (research R7), otherwise exclude the construct from the generator and record
  it in `results.md` Deviations.

**Checkpoint**: T021–T022 pass; `python3 tests/regex_diff.py` reports no difference.

---

## Phase 6: User Story 4 — Safe on any pattern and any input (P2)

**Goal**: limits, linear time, `--timeout`.

**Independent Test**: the cases below and `tests/regex_speed.py`.

- [X] T027 [P] [US4] Golden cases in `tests/cases/regex/`: `limits` (run): `parse_regex` of a
  20,000-byte pattern, of `(a{1000}){1000}`, of 201 nested groups: each an `err` with the R9 message;
  `limits_check` (check): the same as literals → E0319; `timeout` (`.cmd` `run --timeout 1 {}`): a
  match of `(a|aa)*c` over a 50,000,000-byte text built in the program stops with R0009 (exit 124).
- [X] T028 [P] [US4] `tests/regex_speed.py`: SC-002 (`(a+)+$`, `(a|a)*b`, `(.*)*x`, `(a*)*b`,
  `(\w+\s?)*$` on 100,000 and 200,000 characters: each under 1 s, ratio below 2.5) and SC-003 (10 MB
  of generated log lines, `(\d{4})-(\d{2})-(\d{2}) \S+ (ERROR|WARN)`, `all_matches`: under 2 s), median
  of 3 runs; added to `make gate` in `Makefile`.
- [X] T029 [US4] `src/regex.c`: the limits of research R9 ("pattern length 10,000 bytes; repetition
  count 1,000; program size after expanding repetitions 20,000 instructions; nesting depth of groups
  200; capture groups 1,000"), each with its message; the match loop spends `rt_budget` and calls
  `rt_poll` (`rt_budget -= units; if (rt_budget <= 0) rt_poll();`, one unit per 64 code points times
  the program length / 64, at least 1).
- [X] T030 [US4] `src/regex.c`: the first-byte set and the anchored start (research R2); `is_match`
  without capture copying. Run `tests/regex_speed.py`; if SC-003 fails, profile and record the figure
  and the next step (`ponytail:` lazy DFA) in `results.md`.

**Checkpoint**: T027 passes; `tests/regex_speed.py` reports no failure.

---

## Phase 7: User Story 5 — Flags (P3)

**Goal**: `(?i)`, `(?m)`, `(?s)`, scoped `(?i:…)`, `(?-i)`.

**Independent Test**: the run case below and the flags added to the differential generator.

- [X] T031 [P] [US5] Golden run case `tests/cases/regex/flags`: `(?i)error` on `Error`/`ERROR`;
  `(?m)^\d+$` over three lines (each all-digit line one match); `(?s)a.b` and `a.b` on `"a\nb"`;
  `a(?i:b)c` on `aBc` and `ABC`; `(?i)é` does not match `É` (ASCII folding); `$` without `(?m)` does not
  match before a final `\n`.
- [X] T032 [US5] `src/regex.c`: flag groups (contracts/pattern.md `flags`): `i` expands ASCII letters to
  two-case classes, `s` makes `.` `ANY_NL`, `m` selects the multiline assertions; scoped `(?flags:…)` and
  `(?-flags)`; an unknown flag letter is E0319.
- [X] T033 [US5] `tests/regex_diff.py`: the generator emits flags; re-run with no difference.

**Checkpoint**: T031 passes; the differential test is clean with flags.

---

## Phase 8: Polish and verification

- [X] T034 [P] `docs/reference.md`: the std table row of contracts/library.md "Reference text";
  `python3 tests/ref_examples.py`; record the token growth (≤ 150) in `results.md`.
- [X] T035 [P] `site/docs/guide/standard-library.md`: a `## std.regex` section with one runnable example
  (validate, extract, replace) and its output, the escaping rule and a pointer to `vo doc std.regex`;
  `python3 tests/site_examples.py`.
- [X] T036 [P] `docs/roadmap.md`: spec 025 row **Done**, the "Regular expressions" gap row removed, a
  section with what was built and "Not done (on purpose)": Unicode classes and case folding, lookaround
  and backreferences, replacement by a function, streaming, a regex literal. `README.md`: status line
  and spec range (001–025).
- [X] T037 [P] `docs/security.md`: one paragraph: patterns from data (`parse_regex`) are safe to run on
  hostile input (linear time, the R9 limits, the bounded cache).
- [X] T038 Golden cases updated by the new std module (e.g. `tests/cases/doc/all*.out` gain
  `std.regex`); review each diff before accepting (`tests/run.sh --accept`).
- [X] T039 `make lint`.
- [X] T040 `make instrumented`; then `VO_BIN=build-instrumented/vo python3 tests/regex_diff.py --quick`
  and `tests/apply_fixes.py`.
- [X] T041 The collector stress build over `tests/cases/regex` (`tests/run.sh --stress 64
  tests/cases/regex`): the vo side allocates `Match` records in loops.
- [X] T042 `make ARCH=aarch64` and `make ARCH=riscv64`; `python3 tests/regex_diff.py --cross build/vo
  build-aarch64/vo build-riscv64/vo`.
- [X] T043 Fuzz the parser: `tests/regex_fuzz.py` (or a mode of `regex_diff.py`): 100,000 random byte
  strings as patterns through `parse_regex`: no crash, every result `ok` or `err`; under the
  instrumented build too.
- [X] T044 Every `ponytail:` comment of this change names its ceiling and upgrade (`src/regex.c`).
- [X] T045 `tests/gc/baseline.py` and other suites that count bench2 or std items: no change expected;
  fix only real breakage.
- [X] T046 `make test` on x86-64 (record make's own exit status, not a wrapper's).
- [X] T047 `make ARCH=aarch64 test`, then `make ARCH=riscv64 test`, one at a time.
- [X] T048 Fill in `results.md` (each SC with its figure); set the spec status to Implemented; mark
  every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001–T003): in parallel.
- **Phase 2**: T004 (failing case) and T006 (decoder refactor) in parallel with T005; then T005 → T007
  → T008 → T009 → T010 → T011 (the engine file is sequential). Blocks every story.
- **US1** (T012–T013): after Phase 2.
- **US2** (T014–T020): after Phase 2 (and T006). T017 (`src/lex.c`) is independent of T018–T019.
- **US3** (T021–T026): after US1 (the `Match` builder). T025 needs T018 (the check file).
- **US4** (T027–T030): after Phase 2; T029–T030 touch `src/regex.c` (not in parallel with T032).
- **US5** (T031–T033): after Phase 2 and T023 (the generator).
- **Polish**: T034–T037 in parallel after the stories; T038 → T039 → T040 → T041 → T042 → T043 →
  T044 → T045; T046 → T047 → T048 last.

## Parallel Examples

- Phase 2: T004 and T006 together while T005 is written.
- After Phase 2: T012 (US1 cases), T014, T015, T016 (US2), T027, T028 (US4) together.
- US3: T021, T022 and T023 together, then T024 → T025 → T026.
- Polish: T034, T035, T036 and T037 together.

## Implementation Strategy

- **MVP**: Phases 1–3: compile, test and extract with groups (validation and field extraction).
- **Then**: US2, which makes every literal-pattern mistake a check error (and fixes `"\d"` for every
  string in the language).
- **Then**: US3 (replace, split, the Python differential test) and US4 (limits, speed).
- **Last**: flags, docs, fuzzing, the three platforms.
