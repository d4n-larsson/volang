# Results: Spec Annotations on Tests

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | Every test and property of the examples (17: `expenses` 5, `status-board` 4, its service 2, `notes-api` 6) carries one `@spec` line naming requirements listed in its README; `vo query specs main.vo` in each lists 100% of those IDs with the right tests; `vo test` ends with a line per ID. |
| SC-002 | met | A program with 10 requirements (`FR-001`…`FR-010`) and tests for 8: one `vo query specs --json` run and a five-line comparison with the ID list finds exactly `FR-004` and `FR-009`. `examples/notes-api` shows the same in its README (FR-006 has no test). |
| SC-003 | met | `tests/cases/spec/all_mistakes.vo`: one check reports all six mistakes (E0116 with the fix `spec`, E0118 twice, E0117, E0119 with the merged annotation, W0101 with the repeat dropped); `tests/apply_fixes.py`: every fix resolves its diagnostic. |
| SC-004 | met | 3 isolated agent runs (`claude -p`, no tools, empty directory; the reference, a 4-test program and 3 requirements): 3/3 annotated every test on the first try, checked clean, and added nothing but `@spec` lines. |
| SC-005 | met | Reference ~10,710 → ~10,818 tokens (+108, limit 200); the full gate on all three platforms (below); unannotated programs give the same results (every existing golden output unchanged except the two `vo query` usage messages, which now name `specs`). |

## US1: annotating tests

- `@` is a token; `@NAME(STRING, ...)` is an item of its own (`N_ANNOTATION`) placed before the item it
  annotates, like a comment, so the test's own span and every reported position stay as they were.
- The checker validates every annotation in one pass: E0116 `unknown_annotation` (fix to `spec` within
  two letters), E0117 `misplaced_annotation` (on another item; also at parse time when a comment or the
  end of the file follows), E0118 `invalid_spec_id`, E0119 `repeated_annotation` (merged fix, offered only
  when nothing but `@spec` annotations sit in between), W0101 `repeated_spec_id` (fix without repeats).

## US2: reports

- `vo test --json`: `"spec":[...]` (sorted, each once) in annotated tests' records; `spec_result` records
  per ID just before the summary record. Text: ` [ID, ...]` at the end of a not-passing test's line, and
  `spec ID: N tests, all passing` / `K not passing` lines just before the summary line.
- The tally uses a fixed table of the parser's strings: tests run on a fresh heap each, so nothing is
  allocated while they run (`ponytail:` limits of 4,096 IDs and 64 per test).

## US3: the query

- `vo query specs ENTRY` (text and `--json`): every ID of the program's tests and properties, sorted, each
  with its tests in source order; standard-library and package files are left out (`query/specs_package`).
  It runs nothing; results come from `vo test` (research R3, spec FR-005/FR-006 after the analysis).

## US4: formatting and editors

- `vo fmt` prints each annotation on its own line right above its item, IDs sorted, below any doc comment.
- IntelliJ plugin 0.1.7: `@NAME` is one `ANNOTATION` token. **Another session had already added the same
  support** (version 0.1.7, `specAnnotation` test) while this one ran; the duplicate declarations this
  session added were removed and the other session's version kept (16 plugin tests pass).

## Final gate

| Check | Result |
|---|---|
| `make gate` (x86-64) | passed: golden, step, catalog, reference, fuzz, examples; ubtrap; diff-asm 2,000 (0 disagreements); gc-check; speed geometric mean 0.902 (limit 1.10) |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | passed |
| `make instrumented` | passed (496 of 496) |
| `make diff-asm COUNT=10000` | 0 disagreements |

## Deviations

- **Annotations are items, not part of the test's record** (the plan said the test's side record): an
  annotation must also be found, and reported, on records and imports, which have no side record.
- **The per-ID lines come just before the summary** (the contract said "ends with"), so the JSON summary
  record stays last.
- **The examples' recordings were not re-recorded**: annotating their tests changed the program text, so
  they replay with the "program differs" note; their output is unchanged.

## Follow-ups

- More annotations (for example deprecation), each with its own spec, if wanted.
