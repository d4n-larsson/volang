# Research: Spec Annotations on Tests

## R1: Syntax and where annotations live

- **Decision:** `@` is a new punctuation token. An annotation is `@NAME(STRING, ...)`; any number may
  stand in front of a top-level item, separated only by whitespace (a comment between an annotation and
  its item, or the end of the file after it, is E0117). The parser stores them on the item's side record
  (`Fx`, which tests and properties already have): name, IDs and spans. The item's own span still starts
  at its keyword, so a test's reported position does not move.
- **Rationale:** the user chose annotations; keeping them out of the item span leaves every existing
  report and position unchanged.

## R2: Validation in the checker

- **Decision:** the parser accepts any `@NAME(...)` with string arguments (other argument forms are the
  usual syntax error); the checker validates, so all mistakes are reported in one pass:
  - **E0116** `unknown_annotation`: a name not in the set; fix: the closest known name (edit distance ≤ 2).
  - **E0117** `misplaced_annotation`: on an item other than `test`/`prop`, or not followed by an item.
  - **E0118** `invalid_spec_id`: an ID not matching `PART(/PART)*`, PART = `[A-Za-z0-9_-]+`, or `@spec()`
    with no IDs.
  - **E0119** `repeated_annotation`: two `@spec` on one item; fix: one annotation with all IDs.
  - **W0101** `repeated_spec_id`: an ID twice in one annotation; fix: the annotation without the repeat.
- **Rationale:** codes in the syntax range (E01xx) because these are about how the source is written,
  not types; checker-time so a mistake does not stop other diagnostics.

## R3: Reports from `vo test`

- **Decision:** a test's JSON record gains `"spec":["FR-002","US1/AC1"]` (sorted; absent when not
  annotated). The human `FAIL`/`ERROR`/`TIMEOUT` line ends with ` [FR-002, US1/AC1]`. When at least one
  test is annotated, `vo test` ends with one summary per ID: text `spec FR-002: 2 tests, 1 not passing`,
  JSON `{"v":1,"kind":"spec_result","id":"FR-002","tests":2,"passed":1}`.
- **Rationale:** results need running the tests, which `vo test` does; the query (R4) stays static like
  every other query. This satisfies FR-006's "when asked, their results" through `vo test` (recorded as a
  deviation from the spec's wording).

## R4: The query

- **Decision:** `vo query specs ENTRY`: every ID referenced in the program's files (the entry and what it
  imports), sorted, each with its tests: text `FR-002  main.vo:14 test "adds a note"`, one line per test;
  JSON one record per ID `{"v":1,"kind":"spec","id":"FR-002","tests":[{"name":...,"kind":"test","file":...,"start":{...}}]}`.
  No IDs: no lines, exit 0.

## R5: Formatting

- **Decision:** `vo fmt` prints each annotation on its own line directly above its item, as
  `@spec("A", "B")`, IDs sorted by bytes, repeats kept (the checker reports them). Doc comments stay
  above annotations.

## R6: The IntelliJ plugin

- **Decision:** `@` followed by a name is one `ANNOTATION` token (`METADATA` color); version 0.1.7.
