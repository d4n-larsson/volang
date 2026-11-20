---

description: "Task list for vo-lang spec 033: Structural Map Keys"
---

# Tasks: Structural Map Keys

**Input**: Design documents from `specs/033-map-keys/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R12), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring, the results file, the benchmark baseline.
- Foundational: the `key` constraint word through parse, fmt and doc; `key_why`.
- US1 (P1) record and variant keys (checker rule, E0309, runtime hash and equality); US2 (P2) generic
  code with `K: key`; US3 (P2) reports, tasks, properties, JSON; US4 (P3) documentation.
- Polish: speed, three platforms, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/map_keys/<name>.vo` (`.cmd` `check --json {}` for diagnostics, `run {}`
  or `test {}` otherwise); accept with `tests/run.sh --accept PATH` after reading the diff.
- **Catalog**: E0309's `docs/errors.md` entry is rewritten in T010; `033-map-keys` goes into
  `tests/catalog.py`'s spec list there. Message texts: contracts/diagnostics.md.
- **Unchanged programs**: no golden case outside the ones this list names may change (FR-004, SC-001);
  the two named changes are `tests/cases/check/map_key` and `tests/cases/bytes/type_errors` (R12).
- **Reference budget**: `python3 tests/ref_examples.py` after each reference change; at most +150 tokens
  (from about 14,545).
- **Between edits**: `make quick AREA=map_keys` (T001); `make test` once per change, through
  `tools/progress.py run test`; the gate only when asked.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `map_keys` (cases `map_keys`, `check`, `bytes`, `generic`,
  `prop`, `tasks`, `test`, `fmt`, `doc`; scripts `catalog.py`, `apply_fixes.py`, `ref_examples.py`,
  `doc_conformance.py`) listed among the known areas; `src/rt_map.c` maps to it.
- [X] T002 [P] `specs/033-map-keys/results.md`: SC-001–SC-006 pending, Verification, Deviations (empty).
- [X] T003 Benchmark baseline before any C change: `make` then `cp build/vo build/vo-before`;
  `tests/map_keys_bench.py OLD NEW` (R10): 1,000,000 inserts, lookups (`get_or`) and removes with `int`
  keys and with `str` keys run on both binaries, paired medians of five, NEW/OLD ratio printed and
  failing above 1.05; the record-key program (`record P { x: int; y: int; }`) against the same with
  `str(x) + "," + str(y)` keys runs on NEW only when NEW accepts it (skipped with a note on OLD). Docstring
  says timing-sensitive, not part of `make test`.

---

## Phase 2: Foundational (blocking)

- [X] T004 Golden cases first: `tests/cases/fmt/map_keys.vo` (+ `expected/`, code 0) with
  `fn count[K: key](xs: list[K]) -> int` written with irregular spacing, formatted to `K: key`;
  `tests/cases/map_keys/doc` (`vo doc` of a module exporting `count[K: key]` shows `[K: key]`).
- [X] T005 `src/vo.h`: `#define TPC_KEY (4) /* spec 033: usable as a map key */`; `src/parse.c`: the
  constraint word `key` beside `ordered`/`effects`/`json`, and the parse error text "`ordered`,
  `effects`, `json` or `key`"; `src/fmt.c` line 213 prints `key`; `src/doc.c` prints `: key`. Every other
  switch on `TPC_*` (`grep -n TPC_ src/*.c`) checked: `key` behaves like no constraint except where T007
  and T013 use it.
- [X] T006 `src/check_generic.c`, `src/vo.h`: `Str *key_why(Type *t, Str *path, long depth)` per R2 and
  data-model.md ("`int`, `bool`, `str`, `bytes` | yes"; record "when every field's type is a key"; variant
  "when every case's value type is a key; cases without a value always are"; type parameter "when
  declared `K: key`"; everything else no), with its own seen-list for recursive types and generic
  members substituted as `json_why` does; reasons worded as contracts/diagnostics.md; `TY_ERROR` is a
  key (no cascades). Exported for `src/check.c`.
- [X] T007 Accept T004; `make quick AREA=map_keys`.

**Checkpoint**: `K: key` parses, formats and documents; nothing uses it yet.

---

## Phase 3: User Story 1 - A record or variant is a map key (P1) 🎯 MVP

**Goal**: `map[Point, int]`, `map[Status, V]` work with every map operation; non-keys are E0309.

**Independent Test**: contracts/syntax.md's first example prints `1 2` and `true`.

- [X] T008 [P] [US1] Golden cases first (`run {}`), `tests/cases/map_keys/`: `record` (contracts example
  plus `keys(m)` order, `remove`, `len`, re-insert after remove goes last), `variant` (`Status.open`,
  `Status.closed("x")` distinct; two `closed("x")` the same), `nested` (record holding a record and a
  variant), `bytes_bool` (`map[bytes, int]` via `std.bytes`, `map[bool, int]`), `generic_record`
  (`map[Option[int], str]` with `std.option`), `recursive` (a tree variant as a key), `separator`
  (`P { a: "a,b", b: "c" }` and `P { a: "a", b: "b,c" }` are two keys), `mutated` (a record changed with
  `set` while a key: `has` with old and new values, `len`, `keys` printed; no crash), `many` (100,000
  record keys inserted, looked up and half removed; count printed).
- [X] T009 [P] [US1] Golden cases first (`check --json {}`), `tests/cases/map_keys/`: `bad_float`,
  `bad_list`, `bad_field` (record with a `float` field: the path), `bad_list_field` (`items` is a list),
  `bad_result`, `bad_func`, `bad_map` (`map[map[str, int], int]`); messages from contracts/diagnostics.md; spans on the key type node.
- [X] T010 [US1] `src/check.c` `resolve_type` `TY_MAP`: replace the `int`/`str` test with `key_why`;
  E0309 "`$1` cannot be a map key: $2." (`float` alone: "key by `str(x)` or a scaled `int`"); drop the
  `bytes` special message. `docs/errors.md` E0309: summary "A map key type contains a part that cannot
  be compared as a key.", "Fixable: sometimes", example `map[float, int]`, fixed `map[int, int]` with
  scaled values; `033-map-keys` in `tests/catalog.py`'s spec list.
- [X] T011 [US1] `src/rt_map.c`, `src/vo.h`: `long key_hash(long key, Type *t)` per R4 (`int`, `bool`,
  `str` through today's `map_hash`; else FNV-1a over length-prefixed `str`/`bytes`, fields in order, tag
  then value; `TY_PARAM` and generic instances through `rt_concrete`); `map_find` and `map_set` use it;
  `map_find` compares with today's code for `int`, `bool`, `str` and `values_equal` otherwise (R5). No
  allocation on the hash or compare path. `map_hash` keeps its signature (`src/check.c:268` uses it). A
  `ponytail:` comment at `map_set` names the limit (a record changed while it is a key is not detected,
  R7) and the way out (copy keys on insert).
- [X] T012 [US1] Move `tests/cases/check/map_key.vo` to a `float` key and re-accept; re-accept
  `tests/cases/bytes/type_errors` (its `map[bytes, int]` line no longer E0309); accept T008 and T009;
  `make quick AREA=map_keys`.

**Checkpoint**: record and variant keys work end to end in non-generic code.

---

## Phase 4: User Story 2 - Generic code keyed by a type parameter (P2)

**Goal**: `fn count[K: key](...) -> map[K, int]` works for every key type; misuse is E0309 with a fix.

**Independent Test**: `count` called with `int`, `str` and a record runs; with `float` it is E0309 at
the call.

- [X] T013 [P] [US2] Golden cases first, `tests/cases/map_keys/`: `generic` (`run {}`: `count[K: key]`
  with `int`, `str`, `Point`, `Status`; a generic function calling another with `K` passed on),
  `generic_record_decl` (`record Index[K: key, V] { by: map[K, V]; }` used with a record key),
  `generic_bare` (`check --json {}`: `map[K, int]` with a bare `K`, the fix appending `: key`),
  `generic_ordered` (`K: ordered`: no fix, message from contracts/diagnostics.md), `generic_call` (a call
  inferring `K` as `float`, and `Index[float, int]`: E0309 at the argument / type argument),
  `generic_rename` (`vo edit rename` of a `K: key` parameter keeps `: key`), `generic_query`
  (`vo query refs` on a `K: key` parameter finds its declaration and every use).
- [X] T014 [US2] `src/check_generic.c` `check_targ`: a `TPC_KEY` branch beside `TPC_JSON` reporting
  E0309 through `key_why`; `key_why`'s `TY_PARAM` reason gives the bare-parameter fix (as the `json`
  branch does) and no fix for a parameter with another constraint.
- [X] T015 [US2] Accept T013 after reading diffs; `python3 tests/apply_fixes.py` covers the new fix;
  `make quick AREA=map_keys`.

---

## Phase 5: User Story 3 - Everything that shows or tests maps handles the new keys (P2)

**Goal**: reports, R0004, tasks, properties and JSON behave with the new keys (R8).

**Independent Test**: a failing `expect_eq` of record-keyed maps prints keys in vo syntax.

- [X] T016 [P] [US3] Golden cases first, `tests/cases/map_keys/`: `report` (`test {}`: a failing
  `expect_eq` of two `map[Point, int]` differing in one key), `missing` (`run {}`: R0004 prints
  `Key Point { x: 1, y: 2 } is not in the map.`), `prop` (`test {}`: a property over `map[Point, int]`
  that fails and shrinks), `tasks` (`run {}`: `concurrent_map` returning `map[Status, int]`), `json`
  (`check --json {}`: `to_json` of `map[Point, int]` is E0328), `query` (`vo query type-at` on a
  `map[Point, int]` variable), `trace` (a step case like `typed_errors/report_trace`: `vo run --trace trace.jsonl main.vo` of a program updating a
  `map[Point, int]` shows the key in vo syntax).
- [X] T017 [US3] Accept T016 after reading diffs; code changes in `src/rt_render.c`, `src/task.c`,
  `src/prop.c` or `src/rt_test.c` only where a case shows a consumer bypassing the key type.

---

## Phase 6: User Story 4 - Programs stop building keys by hand (P3)

**Goal**: the rule is documented where agents read it.

**Independent Test**: the reference states the rule with a runnable example, under 15,000 tokens.

- [X] T018 [P] [US4] `docs/reference.md`: the `map` row ("`K` is a key: `int`, `bool`, `str`, `bytes`,
  or records and variants of those (E0309)"); the `bytes` row drops "not a map key"; `K: key` in the
  type-parameter paragraph beside `ordered` and `json`; the maps paragraph drops "(keys are `int` or
  `str`)"; the collections hazard sentence covers map keys; one runnable record-key example;
  `python3 tests/ref_examples.py` (≤ +150 tokens); `tests/doc_conformance.py` passes.
- [X] T019 [P] [US4] `site/docs/guide/lists-and-maps.md`: "Keys made of several values" with a runnable
  example (a record key; why not `str(x) + "," + str(y)`); `python3 tests/site_examples.py`.
- [X] T020 [P] [US4] `docs/roadmap.md` (row and section for 033; the "Tuples, more map keys" row: keys
  done, tuples declined, records cover them), `README.md` feature list, `docs/review-2026-11/README.md`
  "Fixed since the review" row.
- [X] T021 [US4] SC-004 search (`grep` for map keys joined into a `str` in `examples/` and `std/`) and
  record the result in results.md (R11: none expected; `bench2/` fixtures not edited).

---

## Phase 7: Polish & cross-cutting

- [X] T022 SC-002/SC-003: `python3 tests/map_keys_bench.py build/vo-before build/vo`; figures in
  results.md.
- [X] T023 `make test` (via `tools/progress.py run test`), then `make ARCH=aarch64` and
  `make ARCH=riscv64` with `tests/run.sh tests/cases/map_keys tests/cases/check tests/cases/bytes
  tests/cases/prop tests/cases/tasks tests/cases/fmt/map_keys` under each (byte-identical outputs); not
  the gate unless asked.
- [X] T024 `specs/033-map-keys/results.md`: SC-001–SC-006 with measurements, Verification, Deviations;
  spec status.

---

## Dependencies & Execution Order

- Setup (T001–T003) → Foundational (T004–T007) → US1 (T008–T012).
- US2 (T013–T015) needs US1 (the runtime must hash keys in generic frames). US3 (T016–T017) needs US1.
  US4 (T018–T021) needs US1 and US2 for its examples.
- Polish needs everything. T003 must run before T011 (the baseline binary).

## Parallel opportunities

- T002 with T001 and T003; the golden-case tasks marked [P] (T008, T009, T013, T016) touch only new
  files and can be written ahead of their code.
- T018, T019 and T020 are documentation in different files.

## Implementation Strategy

- **MVP**: Phases 1–3 (T001–T012): record and variant keys in ordinary code, non-keys refused.
- **Then** US2 (generic code), US3 (tools, mostly cases), US4 (docs), each testable alone, and the Polish
  phase.
- `make quick AREA=map_keys` between edits; `make test` once at T023.
