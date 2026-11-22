---

description: "Task list for vo-lang spec 044: Decimal (exact decimal numbers)"
---

# Tasks: Decimal (exact decimal numbers)

**Input**: Design documents from `specs/044-decimal/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R7), data-model.md, contracts/decimal.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden case and its group in `tests/decimals.py`
land first and fail before its code.

**Organization**:
- Setup: the module registered, the test script, the area wiring.
- Foundational: the two types and the private helpers every function uses.
- US1 (P1) text and exact sums; US2 (P1) multiply, divide, round; US5 (P1) stopping instead of a wrong
  result (it verifies US1 and US2, so it follows them); US3 (P2) compare and convert; US4 (P2)
  allocation; US6 (P2) the example and documentation.
- Polish: three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Types, signatures, error messages, the allocation rule**: contracts/decimal.md. Rounding table and
  text form: data-model.md. Algorithms: research.md R2 (overflow), R3 (division), R4 (`to_float`),
  R5 (`requires`).
- **Golden cases**: a case needs a `.code` file to be discovered; run one directory per call of
  `tests/run.sh`; accept with `--accept` and review with `git diff`/`git status tests/cases`.
- **No existing golden case may change**, except `tests/cases/doc/all.out` and `all_json.out` (the
  module listing).
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`
  (results as `EXPR == ok(v) is true`); private functions have none.
- **Contracts**: every exported function is written with its `requires places_0_to_18(...)` clauses
  (R5) from the start, on each `Decimal` and `places` argument.
- `make quick AREA=decimal` between edits; `make test` once, in T020; the gate is not run.

## Phase 1: Setup

- [X] T001 Create `std/decimal.vo` with only its header comment; add it to `STD_FILES` in `Makefile`
  and `"decimal"` to `doc_modules` in `src/doc.c`; add `decimals.py` to the test list in `Makefile`;
  add area `decimal` to `tools/quick.sh` (golden areas `run check doc`; scripts `ref_examples.py
  doc_conformance.py decimals.py`; file mapping for `std/decimal.vo`, `tests/decimals.py`); `make` builds.
- [X] T002 Create `tests/decimals.py` on the pattern of `tests/bits.py` (`--group`, `SLOW`, `case()`,
  temp directory under `build/tmp`), with groups `arith`, `round`, `text`, `alloc`, `speed` as in
  research.md R6: each writes operand lines, runs one vo driver that prints a line per case, and
  compares with Python's `decimal` (context precision 60). Cases whose exact result does not fit
  (units outside 64 bits or places over 18) are run one per process, at most 200 per group, and must
  stop with R0001. Operands mix places 0 to 18 and the extreme units. All groups fail now.

## Phase 2: Foundational

- [X] T003 In `std/decimal.vo`: `export record Decimal { units: int; places: int; }` ("places" 0 to 18;
  the doc comment says that `==` compares units and places, and names `equal` and `normalize`),
  `export variant Rounding { half_even; half_up; down; up; floor; ceiling; }` with a doc comment per
  case, and the private helpers: `places_0_to_18(n) -> bool`, `pow10(n)`, and `decimal(units, places)`
  and `from_int(n)` (exported, with `requires places_0_to_18(places)`).

## Phase 3: US1 - Carry and print amounts exactly (P1)

**Goal**: text in, exact sums, text out. **Independent test**: `tests/decimals.py --group text` and the
`add`/`sub` part of `--group arith` pass.

- [X] T004 [US1] Golden case `tests/cases/run/decimal.vo` (+`.code`, `.out` written by hand from the
  spec): US1 scenarios 1 to 5 and the edge cases on text (`-0.00`, `-0.05`, `007.50`, grouping of
  `-1234567.80` and of `999.5`, every rejected text of scenario 2). Fails now.
- [X] T005 [US1] `parse_decimal` (strict form of data-model.md; error `invalid decimal: TEXT`; a value
  that does not fit is an error, not a stop), `decimal_text`, `grouped_text` in `std/decimal.vo`.
- [X] T006 [US1] `add`, `sub`, `negate`, `absolute`, `total` in `std/decimal.vo` (places `max`; `int`
  operators so that overflow is R0001; `total([])` is 0 at 0 places).

## Phase 4: US2 - Multiply, divide and round (P1)

**Goal**: exact products; quotients and rounding by a stated mode. **Independent test**:
`tests/decimals.py --group round` passes in all six modes.

- [X] T007 [US2] Extend `tests/cases/run/decimal.vo` with US2 scenarios 1 to 4 and the rounding table
  of data-model.md; add `tests/cases/check/decimal_no_mode.vo` (a `round_to` call without a mode is a
  check error, scenario 6). Fail now.
- [X] T008 [US2] `mul` (places over 18 stop with R0001 through a private `overflow()`), and the private
  rounding decision (mode, sign, parity of the quotient, remainder zero / below / at / above half,
  compared as `r` against `d - r`) in `std/decimal.vo`.
- [X] T009 [US2] `round_to` in `std/decimal.vo` on that decision.
- [X] T010 [US2] `div` in `std/decimal.vo` as research.md R3: digit-by-digit long division, the slow
  path for divisors above 922337203685477580, the `k < 0` case with a sticky remainder, magnitudes as
  non-positive values.

## Phase 5: US5 - Nothing overflows silently (P1)

**Goal**: a result that does not fit stops; one that fits never does. **Independent test**: the
one-per-process overflow cases of `tests/decimals.py` and three golden cases.

- [X] T011 [US5] Golden cases `tests/cases/run/decimal_overflow` (R0001 from `mul`),
  `decimal_div_zero` (R0002), `decimal_bad_places` (R0012 for `new Decimal { units: 1, places: 19 }`
  passed to `add`), and in `run/decimal.vo` the fitting result with a large intermediate
  (`1.000000000000000000 / 3` to 18 places) and the compare of 9000000000000000000 with
  0.000000000000000001 (after T012).

## Phase 6: US3 - Compare, convert and use as data (P2)

**Goal**: ordering by value, conversions. **Independent test**: `tests/decimals.py --group arith`
(compare, equal, normalize) and `--group text` (to_int, to_float) pass.

- [X] T012 [US3] Scenarios first, failing; then: `compare` (integer parts first, then fractions scaled to 18 places: never stops),
  `equal`, `is_zero`, `is_negative`, `normalize` in `std/decimal.vo`; scenarios 1, 2 and 5 in
  `tests/cases/run/decimal.vo`, including a `map[Decimal, int]` keyed by `normalize`.
- [X] T013 [US3] Scenarios first, failing; then: `to_int` (error `not a whole number: TEXT`) and `to_float` (`__text_float` on the
  text; try that call first and use R4's fallback if a module cannot make it) in `std/decimal.vo`; scenarios 3 and 4 in the golden case; golden case
  `tests/cases/check/decimal_pure.vo`: the functions used in an `ensures` clause and a `prop`.

## Phase 7: US4 - Split without losing a unit (P2)

**Goal**: parts that sum to the amount. **Independent test**: `tests/decimals.py --group alloc`.

- [X] T014 [US4] Scenarios first, failing; then: `allocate` and `split_evenly` in `std/decimal.vo` by the allocation rule of
  contracts/decimal.md (share `(u / R) * ri + (u % R) * ri / R`; ratios summing to more than
  2147483647 are an error; the most negative units handled); US4 scenarios 1 to 4 and "more parts than
  units" in `tests/cases/run/decimal.vo`.

## Phase 8: US6 - The example and documentation (P2)

- [X] T015 [US6] Doc comments with run examples on every exported item of `std/decimal.vo`;
  `python3 tests/doc_conformance.py` passes; accept the module listing in `tests/cases/doc/all.out`
  and `all_json.out` after reading the diff.
- [X] T016 [P] [US6] Rewrite `examples/expenses/report.vo` as research.md R7 (`amount: Decimal`,
  `decimal(cents, 2)` on reading, `decimal_text`, `total`, `compare`; `money` removed); its output on
  its sample data is byte-identical to before (save the old output first) and `bin/vo test` passes; count the lines of `money` and its
  tests before and after, for SC-004;
  update `examples/README.md` or the site page if they mention `money`.
- [X] T017 [P] [US6] `docs/reference.md`: one `std.decimal` row in the library table;
  `python3 tests/ref_examples.py` stays under 15,000. `site/docs/guide/standard-library.md`: a section
  "std.decimal: exact amounts" with the invoice example of quickstart.md, the `==` and `equal` note,
  and storing a decimal in JSON and SQLite as text; `python3 tests/site_examples.py` and `make site-check` pass.
- [X] T018 [P] [US6] `CHANGELOG.md`, `docs/roadmap.md` (row and section "044: decimal"),
  `docs/review-2026-11/README.md` (row "Exact numbers (language 6)").

## Phase 9: Polish

- [X] T019 Three architectures: `make ARCH=aarch64` and `make ARCH=riscv64`; with `VO_BIN` set, run
  `tests/run.sh tests/cases/run`, `tests/run.sh tests/cases/check` and `python3 tests/decimals.py` on
  each.
- [X] T020 `python3 tools/progress.py run test` once; read `build/progress/test.log`; fix and rerun
  only on failure.
- [X] T021 Write `specs/044-decimal/results.md` (SC-001 to SC-005 with figures, changed cases,
  deviations, what was not verified); set the spec's status to Implemented.

## Dependencies

- T001 → T002, T003 → everything.
- US1 (T004–T006) → US2 (T007–T010) → US5 (T011). US3 (T012, T013) needs only Phase 2 but T011's
  compare case needs T012. US4 (T014) needs T006 and T012. US6 needs all functions.
- T016, T017 and T018 touch different files and can run in parallel after T015.

## Implementation strategy

MVP is US1 and US2: text, sums, products and rounding replace hand-written money code. US5 is their
verification and is done before anything is documented. Stop after any phase with
`make quick AREA=decimal` green.
