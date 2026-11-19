# Results: Regular Expressions

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/regex_diff.py`: 100,000 random (pattern, text) pairs over the supported syntax (classes, shorthands, anchors, groups of all three kinds, alternation, greedy and lazy quantifiers including counted ones, global and scoped flags; texts with `\n` and non-ASCII): match offsets (bytes), every group, `split_on` and `replace_all` equal Python's `re` (ASCII, `$` as `\Z`); 0 differences. `--cross build-aarch64/vo build-riscv64/vo`: identical output on all three platforms. One generated pattern was skipped because Python itself took over 0.2 s on it. Other seeds (7, 23, 42: 50,000 pairs each): 0 differences; seed 11: 1 difference (see Deviations). |
| SC-002 | met | `tests/regex_speed.py`: `(a+)+$`, `(a\|a)*b`, `(.*)*x`, `(a*)*b`, `(\w+\s?)*$` on 100,000 characters: 0.018–0.035 s each; on 200,000: at most 2.42 times as long (limit 1 s and 2.5). |
| SC-003 | met | `tests/regex_speed.py`: all matches of a date-and-level pattern in 10 MB of generated log lines: 0.47 s (limit 2 s), with the right count (64,538). |
| SC-004 | met | Check cases `syntax` (12 kinds of E0319), `unsupported` (13 kinds of E0320), `not_literal` (E0321), `replacement` (E0322, through a direct call and a `let`), `one_pass`, `limits_check`, and `lex/bad_escape` (E0103's new fix); `tests/apply_fixes.py`: 100/100 fixes resolve their diagnostic (now also over `tests/cases/lex`). |
| SC-005 | met | E0319–E0322 in `docs/errors.md` and E0103 now Fixable: yes (`tests/catalog.py`: 119 sections, 0 problems); 22 golden cases in `tests/cases/regex/` cover every acceptance scenario; `tests/doc_conformance.py` runs the example of each of the 10 `std.regex` items. |
| SC-006 | met | `tests/ref_examples.py`: ~12,931 tokens of 15,000 (+29, limit +150). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 818, step 211 + 235 + 35, the self-tests, fuzz, fixes (100/100), catalog (119), reference, site (196 examples), doc conformance (688 calls, 139 examples), `format_diff --quick`, `regex_diff --quick`, `regex_fuzz` (20,000), examples, diff-engines 1,000 (0 disagreements) |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): the same counts, all passed |
| `make lint` | clean |
| `make instrumented` (ASan, UBSan) | 818 golden cases passed; `regex_diff.py --quick`, `apply_fixes.py` and `regex_fuzz.py --count 20000` under it: clean |
| Collector stress build | `tests/run.sh --stress 64 tests/cases/regex tests/cases/format`: 48 passed; `regex_diff.py` (1,000 pairs) with a collection every 8th allocation: 0 differences |
| `tests/regex_fuzz.py` | 100,000 random patterns through `parse_regex`, `is_match`, `all_matches`: no failure |
| `make gate` | not run (only when asked); its new step, `tests/regex_speed.py`, was run: 0 failures |

## Size

- `src/regex.c` (new): 1,199 lines: parser, compiler, Pike VM, compile cache, replacement parser, the
  three primitives (`__re_compile`, `__re`, `__re_repl`).
- `src/check_regex.c` (new): 120 lines; `src/lex.c`: E0103's fix; `src/check_format.c`: the literal
  decoder is shared (`lit_decode`).
- `std/regex.vo` (new): 157 lines with documentation.
- Tests: 22 golden cases in `tests/cases/regex/`; `tests/regex_diff.py`, `tests/regex_speed.py`,
  `tests/regex_fuzz.py`.

## Deviations

- **A third primitive, `__re_repl`**: the replacement text is parsed in C, not in vo, because the std
  code cannot raise R0007 and the checker needs the same parser for E0322.
- **Error text**: run-time pattern errors read `at byte N: MESSAGE` and R0007 `Invalid pattern at byte
  N: MESSAGE` (contracts updated), instead of a trailing "(at byte N)" after a full sentence.
- **Empty iterations follow Python**: Python's backtracking takes one more empty iteration of a loop
  after the last non-empty one, and stops a counted repetition after an empty iteration. A plain Pike
  VM loses both (a state is entered once per step). The engine gives every instruction inside a loop
  body two "entered" marks per step (has the current iteration consumed anything yet, kept in a hidden
  capture slot per loop) and ends a counted repetition after an empty copy (`EMPTYEXIT`); still linear
  (at most twice the states). This removed a difference first recorded in `contracts/pattern.md`
  (`(a|)*` on `aa` now leaves group 1 empty, as Python).
- **One known difference remains**: captures inside nested lazy repetitions where both levels can match
  empty (seed 11, 1 of 50,000 random patterns: `(b*?(...)*){2,}?2`). The match itself is the same; one
  group's text differs. Left for a follow-up: rare, and fixing it needs marks per enclosing loop.
- **Python quirk excluded**: `\B` on an empty text (Python before 3.14 never matches there; vo does).
- **The differential test times out Python** at 0.2 s per pair: random patterns can make Python's
  backtracking engine run for minutes (the problem the spec exists to avoid); such pairs are skipped and
  counted.
- **Bugs found by the tests and fixed before results**: the state marks of a failed position leaked into
  the next one (patterns matching only empty text at later positions found nothing); the cache key was
  allocated without room for its header; the group names and the first-byte scratch arrays lived in the
  collected heap without roots (found by the collector stress build).
- **`make test` gained three fast checks**: `format_diff.py --quick` (spec 024 had left it out),
  `regex_diff.py --quick`, `regex_fuzz.py --count 20000`.
- **`tests/apply_fixes.py` also covers `tests/cases/lex`**, so E0103's fix is applied and re-checked.
- **The engine was written in one piece** (flags included) before the US5 cases, since the parser
  state is shared; every story's cases were still written and run before its tasks were marked.
