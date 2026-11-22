---

description: "Task list for vo-lang spec 046: Iteration (two-variable for, list helpers)"
---

# Tasks: Iteration (two-variable `for`, list helpers)

**Input**: Design documents from `specs/046-iteration/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/iteration.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases and its group in `tests/iteration.py`
land first and fail before its code.

**Organization**:
- Setup: the test script, the area wiring, the empty library module.
- Foundational: the syntax (parser, flag, `FOR_RANGE`, formatter): every story needs it.
- US2 (P1) lists and deques comes before US1 (P1) maps: it needs no new instruction, and the map loop
  is built on the list loop.
- US3 (P2) the warning; US4 (P2) `std.lists`; US5 (P2) tools, moved code, documentation.
- Polish: three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Meaning, syntax, diagnostics, signatures**: contracts/iteration.md and data-model.md. Decisions:
  research.md R1 (tree), R2 (lists), R3 (maps), R4 (errors, W0403), R5 (`std.lists`, `sort_by`), R6
  (budget), R7 (tools), R9 (moving code).
- **Golden cases**: a case needs a `.code` file to be discovered; run one directory per call of
  `tests/run.sh`; accept with `--accept` and review with `git diff`/`git status tests/cases`.
- **No existing golden case may change**, except: `tests/cases/doc/all.out` and `all_json.out` (the
  module listing); cases that print source lines of `std/` files whose loops move (line numbers, the
  lines themselves); the fingerprint line of `record_replay` step cases whose programs import a
  changed module (update it, rerun `python3 tests/steps.py` on those directories).
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`;
  private functions have none. No spec numbers in doc comments shown on the site or on the site.
- `make quick AREA=iteration` between edits; `make test` once, in T022; the gate is not run.

## Phase 1: Setup

- [X] T001 Create `tests/iteration.py` on the pattern of `tests/web_security.py` (`--group`,
  `case()`, temp directory under `build/tmp`, emulation and `CI` detection) with groups `loops`,
  `lists`, `speed` as in research.md R8. `loops`: a generator of programs (seeded; maps, ordered
  maps, lists, deques; bodies made of print, update a value, add a key, remove this or another key,
  remove and put back a later key, push, pop, `break`, `continue`, a nested two-variable loop over the same map; `_` for either
  variable) and a model in Python of contracts/iteration.md that gives the expected output. All
  groups fail now.
- [X] T002 Create `std/lists.vo` with only its header comment; add it to `STD_FILES` in `Makefile`
  and `"lists"` to `doc_modules` in `src/doc.c`; add `iteration.py` to the test list in `Makefile`;
  add area `iteration` to `tools/quick.sh` (golden areas `run check fmt doc`; scripts
  `ref_examples.py doc_conformance.py iteration.py`; file mapping for `std/lists.vo`,
  `tests/iteration.py`); `make` builds.

## Phase 2: Foundational

- [X] T003 Golden cases, failing: `tests/cases/fmt/for_pair` (a file with `for a,b in m`, `for _, v
  in m`, `for i, _ in xs` formatted to the canonical spacing) and `tests/cases/parse/for_pair_range`
  or the nearest area (`for a, b in 0..3` is E0301; `for a, in m` and `for , b in m` are the existing
  syntax error).
- [X] T004 `src/vo.h`: `NF_PAIR` (a free flag bit), `FOR_RANGE(st)`; update the comment of `N_FOR`.
  `src/parse.c`: after the first binding accept `,` and a second binding (`_` or a name) into `C`
  with `NF_PAIR`; `..` after a pair is reported by the checker (T006), not here. Replace the eight
  tests of `st->c.n` on an `N_FOR` in `src/check.c`, `src/check_structure.c`, `src/fmt.c`,
  `src/compile.c`, `src/vm.c` with `FOR_RANGE`. `src/fmt.c`: print `for A, B in X`. T003's fmt case
  passes.

## Phase 3: US2 - A list's elements with their positions (P1)

**Goal**: `for i, x in xs`. **Independent test**: `tests/cases/run/for_pair_list` and the list and
deque programs of `tests/iteration.py --group loops`.

- [X] T005 [US2] Golden cases, failing: `tests/cases/run/for_pair_list.vo` (story 2 scenarios 1 to 3:
  numbering lines; a deque; a body that pushes (the new elements are not visited), one that pops
  (the loop ends early) and one that changes a later element (the new value is seen); `_` in each place; `break`,
  `continue`, `return`; nested loops; a list of records) and `tests/cases/check/for_pair_errors.vo`
  (two variables over a `str`, a range, an `ordered_set`, a `priority_queue`: E0301 each with the fix
  that removes the first variable; over an `int`: E0301 without a fix; `for x, x in xs`; a variable named like a builtin (`for len, x in xs`); assigning to
  either variable; an unused first or second variable with its `_` fix).
- [X] T006 [US2] `src/check.c` `check_for`: with `NF_PAIR`, a list or deque gives `int` and the
  element type, a map or ordered map the key and value types; anything else, and a pair with a range,
  is E0301 with the message of research.md R4 and, over text, a range, an `ordered_set` or a
  `priority_queue`, the fix. Two consecutive slots (`E`, `E + 1`) whether named or `_`.
  `src/vm.c` `fl_stmt`: the second variable with its type.
- [X] T007 [US2] `OPC_FOR_VALUE` in `src/vo.h` and `src/vm.c`, for a list: slot `a` = the element
  at the index in slot `a - 8` of the list in `b`, or `pc = c` when the index is past the end.
  `src/compile.c` `cfor`: for a pair over a list (a deque after `OPC_SNAPSHOT`), a range loop from 0
  to the length at the start with the index in slot `E`, the list in a temporary, and
  `OPC_FOR_VALUE` first in the body with the loop's exit as its target (R2). T005's run case and the
  list and deque programs of `--group loops` pass.

## Phase 4: US1 - A map's keys and values together (P1)

**Goal**: `for k, v in m`. **Independent test**: `tests/cases/run/for_pair_map` and
`tests/iteration.py --group loops` in full.

- [X] T008 [US1] Golden case, failing: `tests/cases/run/for_pair_map.vo` (story 1 scenarios 1 to 7:
  a word count; an `ordered_map`; an empty map; `_` in each place and both; updating the value of the
  current and of a later key; removing the current, an earlier and a later key; adding keys;
  removing and re-adding the current key (not visited again); removing and putting back a later key
  (visited once, with the new value); record values; record keys; nested loops over
  the same map; `break`, `continue`, `return`; a loop inside a function with `requires` and inside a
  `test`).
- [X] T009 [US1] `OPC_SNAPSHOT` on a `map` gives its keys in insertion order (`map_keys`);
  `OPC_FOR_VALUE` for a map or ordered map: slot `a` = the value of the key in slot `a - 8` in
  the map or ordered map in `b`, or `pc = c` when the key is gone (one lookup; `rt_coll.c` for the
  ordered map). `src/compile.c` `cfor`: the map kept in a temporary, the key list loop with the key
  in slot `E`, `OPC_FOR_VALUE` first in the body with the back-edge as its target (R3), with the
  `ponytail:` comment of R3. T008 and `tests/iteration.py --group loops` pass (1,000 programs, SC-001).
- [X] T010 [US1] `tests/iteration.py --group speed`: a map of a million entries summed with
  `for k in keys(m)` and `m[k]`, and with `for k, v in m`: times (best of 3) and, for each, the smallest `--max-memory` (searched in steps of 8 MiB) under
  which it completes are printed; on x86-64 outside CI the new form must not be slower than the old
  by more than 5% nor need a larger limit (SC-002).

## Phase 5: US3 - The old map pattern points to the new form (P2)

**Goal**: W0403 with its fix. **Independent test**: `tests/cases/check/w0403*` and the fix applied.

- [X] T011 [US3] Golden cases, failing: `tests/cases/check/w0403.vo` (both patterns, with the
  collection a `let` and a parameter: warned; `--fix` output or the fix in JSON as neighbouring cases show fixes) and
  `tests/cases/check/w0403_not.vo` (no warning: no first `let`; the `let` has another index or
  another map; `let` with a type annotation; `var v`; `m` or `xs` a `var`, a field, a call; `keys` of an
  `ordered_set`; a range that does not start at 0 or ends at something other than `len(xs)`). An
  entry for W0403 `for_reads_entry` in `docs/errors.md` after W0402, in the form `tests/catalog.py` checks.
- [X] T012 [US3] `src/check.c`: W0403 on exactly the two shapes of research.md R4, one fix each
  replacing the text from `for` to the end of the `let` statement by `for K, V in M {` or
  `for I, X in XS {`. `tests/apply_fixes.py` and
  `tests/catalog.py` pass; applying the fix to T011's first case gives a file that checks without
  the warning and prints the same.

## Phase 6: US4 - Common list operations exist once (P2)

**Goal**: `std.lists`. **Independent test**: `tests/iteration.py --group lists`.

- [X] T013 [US4] Golden cases, failing: `tests/cases/run/lists.vo` (story 4 scenarios 1 to 6 and the
  edge cases: empty lists, one element, equal keys keeping their order in `sort_by_desc`, text and
  float keys, `min_by`/`max_by` giving the first of equals and `err("empty list")`, `sum` of an
  empty list, `group_by` with a record key and key order of first appearance, a key function that
  prints (effects)), `tests/cases/run/lists_sum_overflow` (R0001), and
  `tests/cases/check/sort_by_param_key.vo` (`sort_by` with a key of a `C: ordered` type parameter is
  accepted; of an unconstrained `C` it is E0313; `group_by` with a key that cannot be a map key is
  the existing error for map keys). The `lists` group of `tests/iteration.py`: each
  function against a Python loop on 10,000 random lists (1,000 under emulation).
- [X] T014 [US4] `src/check_func.c`, the `B_SORT_BY` case: a key whose type is a type parameter with
  the `ordered` constraint is accepted (R5). The entry for E0313 in `docs/errors.md` says so where
  it lists what a `sort_by` key may be; `python3 tests/catalog.py` passes.
- [X] T015 [US4] `std/lists.vo`: the eight functions of contracts/iteration.md, `sort_by_desc` as
  reverse in place, `sort_by`, reverse in place; doc comments with run examples; `python3
  tests/doc_conformance.py` passes; accept the module listing in `tests/cases/doc/all.out` and
  `all_json.out` after reading the diff. `tests/iteration.py --group lists` passes (SC-004).

## Phase 7: US5 - Tools, moved code, documentation (P2)

- [X] T016 [US5] `editors/tree-sitter-vo/grammar.js`: `for_statement` with an optional second
  binding; regenerate the parser and the grammar hash (`make grammars`); a highlight test line for a
  two-variable loop; the TextMate and Vim grammars and the site's highlighter changed only if
  `python3 tests/grammars.py` or the site checks ask for it. `python3 tests/grammars.py` passes with
  the tools present (not skipped).
- [X] T017 [P] [US5] One golden case each on a two-variable loop, in the area where the neighbouring
  cases are: `vo edit` rename of the first and of the second variable, `vo query` references of
  both, `vo test --coverage` (a loop body never run), `vo test --mutate` (a mutant inside the body
  is killed or reported), and one hover request on each of the two variables in `tests/lsp.py` (SC-005).
- [X] T018 [US5] Move the code (R9): apply the W0403 fixes in `std/` and `examples/` (both
  patterns); read the loops of either shape that the warning does not match (a `var` or a field) and
  rewrite by hand those whose bodies do not reassign the collection, listing the rest in the results; replace `has_item` in `std/web.vo` by `std.lists.contains` if the import causes no cycle
  (else leave it and say so in the results). Regenerate `vo.effects` of touched examples only if
  `vo check` asks. `vo check` gives no W0403 in `std/` (checked through a program importing each module) or
  `examples/` (SC-003). Update changed golden outputs and fingerprints as the conventions allow, reading each
  diff.
- [X] T019 [P] [US5] `docs/reference.md` as research.md R6 (the loops row, the sentence about maps,
  the word-count example, one `std.lists` row, `sort_by`'s note if it names the key types; shorten
  the `std.security` row if needed); `python3 tests/ref_examples.py` under 15,000.
- [X] T020 [P] [US5] Site: the loops section of the language guide (`site/docs/guide/`) shows both
  two-variable forms, that a two-variable loop visits what was there when it began while
  `for x in xs` follows the list, and what a map loop does when the body changes the map; `site/docs/guide/standard-library.md` gets a short `std.lists` section
  with the quickstart's example. `python3 tests/site_examples.py` and `make site-check` pass.
  `CHANGELOG.md`, `docs/roadmap.md` (row and section "046: iteration"),
  `docs/review-2026-11/README.md` (row "Iteration (language 7)").

## Phase 8: Polish

- [X] T021 Three architectures: `make ARCH=aarch64` and `make ARCH=riscv64`; with `VO_BIN` set, run
  `tests/run.sh tests/cases/run`, `tests/run.sh tests/cases/check` and `python3 tests/iteration.py`
  on each.
- [X] T022 `python3 tools/progress.py run test` once; read `build/progress/test.log`; fix and rerun
  only on failure.
- [X] T023 Write `specs/046-iteration/results.md` (SC-001 to SC-006 with figures, the loops moved
  and those left with the reason, changed cases, deviations, what was not verified); set the spec's
  status to Implemented.

## Dependencies

- T001, T002 → T003, T004 → everything.
- US2 (T005–T007) → US1 (T008–T010) → US3 (T011, T012). US4 (T013–T015) needs only Phase 2.
- US5: T016 and T017 need US1; T018 needs US3 and US4; T019 and T020 need everything before them.
- T017, T019 and T020 touch different files and can run in parallel.

## Implementation strategy

MVP is the two P1 stories: the loop over lists, then over maps, with the generated programs agreeing
with the model. The warning and the library follow; the code is moved only when both exist. Stop
after any phase with `make quick AREA=iteration` green.
