---

description: "Task list for vo-lang spec 007: standard library and content-pinned packages"
---

# Tasks: Standard Library and Content-Pinned Packages

**Status (2026-11-14):** merged incomplete so that spec 008 (the C port) can start; the open tasks below are deferred until after the port and will be done in C. See results.md "Deferred".

**Input**: Design documents from `specs/007-stdlib-packages/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/library.md, contracts/cli.md, contracts/packages.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution v1.1.0, Principle IV).
- Every story starts with golden or step cases that fail on the current binary.
- Every new code gets a case that `tests/catalog.py` finds.
- Every new reference example runs in `tests/ref_examples.py`.

**Organization**: one phase per user story, in spec priority order.
- **MVP:** Phase 2 plus US1 (`vo doc`) on the small `std.text` skeleton, then US2 (JSON).
- **US3 (packages)** is independent of US1/US2 after Phase 2.
- **US4 (time, text, random)** and **US5 (HTTP)** need Phase 2; US5 also uses US2's JSON in its tests.
- **US6 (evidence)** needs US1–US5.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: US1–US6 from spec.md.

## Conventions

**Carried over** from specs 001–006:
- RV64IMAFD, lp64d, GNU as; the SESI hook checks every assembly edit; every report is fixed or explained (false positives go to `sesi-linter-issues.md`).
- C prototype comments and `.globl` on every assembly function; shared constants in `src/defs.inc`; no libc.
- Golden cases: `tests/cases/<dir>/<name>.vo` with `.out`/`.err`/`.code` (and `.cmd`); step cases: folders with a `steps` file (`tests/steps.py`). Accept outputs only after checking them by hand against the contracts.
- The collector does not scan a/t registers: spill values that must survive an allocation.
- `pkill` patterns are anchored with `^`; `make lint` runs with `</dev/null`.

**Library code is vo** (`std/*.vo`): it must check clean, be `vo fmt`-canonical, and have a comment above every public item: the first line is the one-line description, lines starting with `#     ` form the example (research R7).

**What the definitions are:** library API: contracts/library.md; commands: contracts/cli.md; manifest and hash: contracts/packages.md; codes: contracts/codes.md; layouts: data-model.md; decisions: research.md.

**Regression rule:** after every task that touches `src/` or `std/`, `make test` passes; existing outputs stay byte-identical (FR-020).

**Speed rule:** check `python3 tests/speed.py --baseline build-base/vo` after any runtime change (`rt_*.s`, `num.s`, `sys.s`, `interp.s`, `vm.s`); if a ratio passes 1.05, compare page offsets of hot functions with the baseline before anything else (spec 005/006 results, "layout").

**Shortcuts get `ponytail:` markers:** one HTTP request per connection; IPv4 and one DNS server; JSON depth 512; the 16 MiB response cap; JSON parsing over `chars`.

---

## Phase 1: Setup (Shared Infrastructure)

- [X] T001 Create `std/` (empty `json.vo`, `time.vo`, `text.vo`, `random.vo`, `http.vo` with a header comment each), `tests/cases/{doc,json,time,text,random,packages,http}/` and `specs/007-stdlib-packages/results.md` (SC table SC-001…SC-009 "open", sections Deviations, Follow-ups). Confirm `tests/run.sh` and `tests/catalog.py` pick up the new case directories.
- [X] T002 [P] Move the speed baseline to spec 006: in `Makefile` set `BASE_REV := 1ff166a`, rebuild `build-base` (`git worktree remove build-base/src`, then `make baseline`), run `python3 tests/speed.py --baseline build-base/vo` and record the noise in `results.md`.
- [X] T003 [P] Search `tests/`, `bench/`, `bench2/` and `docs/` for `.vo` files that import a path starting with `std.` or contain a local directory named `std` next to an importing file; record the result in `results.md` ("Name clashes"; expected none). Search for programs whose expected outputs depend on `now_ms` without `--clock-ms` (research R5); list them.

---

## Phase 2: Foundational (Blocking Prerequisites)

**⚠️ CRITICAL:** no user story work can begin until this phase is complete.

- [X] T004 Add to `src/defs.inc`: `FILE_STD` and `FILE_PKG` file-table fields (grow `FILE_SIZE`), `EVK_HTTP 8` and an `EV_PORT` event field (grow `EV_SIZE`), builtin ids for `__http_exchange` and `__float_text`, and the new code strings' places as needed.
- [X] T005 Embed the library (research R1, R2): in `src/data.s`, `.incbin` each `std/*.vo` with a `std_files` table `{path cstr, text, length}` and a lookup `std_file(path str) -> (text, len) | -1`; make `$(BUILD)/data.o` depend on `std/*.vo` in `Makefile`. In `src/loader.s`, an import whose first segment is `std` resolves to the path `std/MODULE.vo`, registered from the table (not the disk) with `FILE_STD` = 1 and `FILE_DIR` = `std/`; an unknown module is E0205. Golden cases `tests/cases/text/std_import.vo` (imports a skeleton function from `std/text.vo`: write `starts_with` and `ends_with` now, in vo, with doc comments) and `tests/cases/text/std_unknown_module.vo` (E0205).
- [X] T006 Library effects (research R6) in `src/check_effects.s` / `src/check_expr.s`: a call whose callee is declared in a `FILE_STD` file needs its effects at the caller with any scope (E0501 with the builtin-style fix when missing; E0503 does not apply). In `src/effects_record.s`, functions of `FILE_STD` files are not listed. Before running (`compile_program` or `run_main`/test start), clear the runtime's declared effect sets of `FILE_STD` functions so their frames inherit the caller's grant; verify every frame-setup path in `src/vm.s` takes the inherit branch for them (read only: do not add instructions to `vm.s` handlers). If a path does not inherit (for example `op_call_fast`, which builds the frame inline), the compiler emits calls of `FILE_STD` functions through the general call instruction instead, so `vm.s` stays unchanged; record which in `results.md`. Golden case `tests/cases/text/std_record.vo` (a program with `vo.effects` that imports `std.text.starts_with`: the record lists no `std` entry). The effect rule's run-time half (scopes through a library call) is tested in US4 (`random`, `clock`) and US5 (`net` scope denial).
- [X] T007 The `__` primitive gate in `src/check_expr.s`: builtin names starting with `__` resolve only inside `FILE_STD` files; elsewhere they are E0201 (`tests/cases/check/std_primitive_outside.vo`).
- [X] T008 Make `now_ms` read CLOCK_REALTIME in `src/rt_effects.s` (keep the monotonic clock for deadlines in `src/interp.s`/`src/rt_io.s`); `--clock-ms` unchanged. Every case T003 listed must stay byte-identical, or its change is recorded in `results.md` as intended with the reason. Speed check (Speed rule).

**Checkpoint:** `make test` passes; `import std.text.starts_with;` works; speed unchanged.

---

## Phase 3: User Story 1 - Exact signatures on demand (Priority: P1) 🎯 MVP

**Goal:** `vo doc [--json] [NAME]` (FR-001–FR-004, contracts/cli.md).

**Independent test:** `tests/run.sh tests/cases/doc` passes; `tests/doc_conformance.py` passes for every listed item.

- [X] T009 [P] [US1] Golden cases in `tests/cases/doc/` (`.cmd` files): `all` (no NAME), `all_json`, `module_text` (`std.text`), `module_text_json`, `item` (`std.text.starts_with`), `item_json`, `builtin` (`print`), `builtin_json`, `unknown_module` (`std.txet`: T0010 with "did you mean std.text"), `unknown_item`, `package_module` (a manifest in the case directory; added in US3, seed it then).
- [X] T010 [US1] `src/doc.s`: `cmd_doc` (dispatch in `src/main.s`, `vo doc` with zero or one positional) checks a synthetic in-memory program that imports every item of every standard file (and of every package module of the nearest `vo.packages` from the working directory, once US3 exists), then prints from the checked declarations: `fn NAME[T, ...](p: T, ...) -> R uses EFFECTS`, records with fields, variants with cases (`ty_name`, the FX record, `fx_item_name`); descriptions and examples from the `N_COMMENT` lines immediately above each item (research R7). Builtins come from the embedded reference's Builtins table (rows `| name | \`sig\` | notes |`).
- [X] T011 [US1] `vo doc --json`: one `doc` record per item (contracts/cli.md: module, name, item, tparams, params `{name,type}`, returns, effects, signature, description, example; fields/cases for types) and a `summary` (`command` "doc", `items`). T0010 `doc_not_found` with the closest name (edit distance ≤ 2 over module and item names); add T0010 to `docs/errors.md`.
- [X] T012 [US1] `tests/doc_conformance.py` (research R12): for every item of `vo doc --json`, generate a program calling it with values of the documented types (a value generator per type: literals for `int`/`float`/`bool`/`str`, `new list[T]`, `new map[K, V]`, `ok(...)`, records with every field, the first variant case; type parameters instantiated with `int`), require a clean check; then for each parameter a call with a wrong-typed argument and a call with one argument missing, each of which must fail to check. Effects are declared at the caller as the doc says. Then run every item's example: each `example:` is written `EXPR is VALUE`; wrap it in a program that prints `EXPR` (with the item's imports and effects), which must check clean and print exactly `VALUE`; an item without an example is a failure. Time `vo doc --json` with no NAME under qemu (must be under 1 s) and record it in `results.md`. Add the script to the `Makefile` `test` list.

**Checkpoint:** `vo doc` works for `std.text`'s skeleton and the builtins.

---

## Phase 4: User Story 2 - JSON (Priority: P1)

**Goal:** `std.json` (FR-006, contracts/library.md).

**Independent test:** `python3 tests/json_corpus.py` passes; `tests/run.sh tests/cases/json` passes.

- [X] T013 [P] [US2] The JSON corpus: `tests/json/valid/*.json` (≥ 100: every value kind, nesting, escapes including `\u` and surrogate pairs, non-ASCII, numbers with exponents and fractions, big integers, `-0`, empty containers, duplicate keys, whitespace) each with `*.expected` (the value as `vo` would print it with `write`), and `tests/json/invalid/*.json` (≥ 100: trailing commas, leading zeros, bad escapes, unterminated strings, lone surrogates, invalid UTF-8, control characters in strings, text after the value, empty input, depth 513) each with `*.expected` (the error message with line and column). Written from RFC 8259's grammar, before the parser.
- [X] T014 [P] [US2] Golden cases in `tests/cases/json/`: `config_update` (parse a file, change a field, `write_json_pretty`), `report` (build an object from records, `write`), `fields` (`text_field`/`int_field`/`float_field`/`bool_field` on present, missing and wrong-kind fields), `numbers` (integer vs number, `as_int` of a fraction is `err`), `nan` (`write` of a NaN `number` is `err`), `deep` (513 levels: `err`).
- [X] T015 [US2] `__float_text` in `src/num.s` (17 significant digits, the existing `float_to_text` with a digit-count argument) and its `btab` entry in `src/rt_call.s`; speed check.
- [X] T016 [US2] `std/json.vo`: the `Json` variant; `parse` (over `chars(s)` with an index; line/column tracking; RFC 8259 exactly; depth ≤ 512; `\uXXXX` with surrogate pairs via `chr`; integers via `parse_int` when they fit, else `parse_float`); `write` and `write_json_pretty` (escaping per contracts/library.md, `__float_text` for numbers, `err` for NaN/infinity); `field`, `at`, `as_*`, `*_field` with the exact error messages of contracts/library.md. Doc comments on every item.
- [X] T017 [US2] `tests/json_corpus.py`: a vo driver program (reads a file, prints `write(parse(...))` or the error) run over the corpus; checks expected values, round trip (`parse(write(v)) == v`), and positions; add to the `Makefile` `test` list. Extend `tests/fuzz.py` (or `tests/fuzz_inputs.py`) with mutated corpus documents: never a crash, timeout or exit 125. Time `parse` of a generated 100 KB document under qemu (must be under 2 s) and record it in `results.md`.

**Checkpoint:** JSON works and is documented by `vo doc std.json`.

---

## Phase 5: User Story 3 - Content-pinned packages (Priority: P1)

**Goal:** `vo.packages`, hashing, resolution, `vo package` (FR-013–FR-017, contracts/packages.md).

**Independent test:** `python3 tests/steps.py tests/cases/packages` passes, including every tamper case.

- [ ] T018 [P] [US3] Step cases in `tests/cases/packages/` (each: a program, a package directory, `steps`): `add_and_use` (`vo package main.vo money shared/money`, then `vo run`), `update` (change a file, `vo check` → E0701 naming both hashes, `vo package` → `updated`), `tamper_byte`, `tamper_add_file`, `tamper_remove_file`, `tamper_rename_file` (all E0701, nothing runs), `missing_dir` (E0702), `bad_manifest` (E0703), `duplicate_name` (E0704), `name_clash` (E0705), `unknown_package` (E0205), `package_imports_std`, `package_imports_package`, `package_effects` (a package function gains `net`: E0504 at the caller; the record shows `@money/...`), `remove` (`vo package --remove`), `json` (`--json` output), `no_runtime_read` (the package directory is made unreadable after `vo check` and before the run step, which re-checks and reports E0702: packages are resolved and verified during the check that precedes every run, US3 scenario 7). At least 20 tamper variants over the four kinds (SC-004).
- [ ] T019 [P] [US3] `src/sha256.s`: `sha256_init/update/final` and one-shot `sha256(p, n, out)` (FIPS 180-4); a temporary test hook or a tiny step case comparing against Python's `hashlib` for the empty string, "abc", the 448-bit and 896-bit vectors, and 1,000,000 × "a".
- [X] T020 [US3] `src/packages.s`: find and parse `vo.packages` (nearest, like `rec_find`; registered with `file_add` for spans; E0703/E0704 with spans); resolve an import's first segment against it in `src/loader.s` (`DIRECTORY/REST.vo`, `FILE_PKG` = the package's name; E0705 when a local directory has the same name next to the importer); on first use of a package list its `.vo` files recursively (sorted byte-wise), hash per contracts/packages.md, compare (E0701 with both hashes, E0702 when the directory is missing or has no `.vo` file), before any of its items are checked. Add E0701–E0705 to `docs/errors.md`.
- [X] T021 [US3] `vo package [--json] ENTRY NAME DIRECTORY` and `--remove ENTRY NAME` in `src/packages.s` + `src/main.s`: compute the hash, write the manifest (created next to ENTRY with the header comment when absent; entries in name order) with the existing safe-write helper, print `added`/`updated`/`unchanged`/`removed` (contracts/cli.md).
- [X] T022 [US3] Effects record (FR-016): `rec_relpath` in `src/effects_record.s` returns `@NAME/relative/path.vo` for `FILE_PKG` files; `vo effects --update` lists package functions; the comparison (`rec_compare`) treats them like local entries.
- [ ] T023 [US3] `vo doc` for packages (T010's hook): module names `NAME.module` from the nearest manifest; accept T009's `package_module` case.

**Checkpoint:** packages work, every tamper is caught, and their effects are reviewable.

---

## Phase 6: User Story 4 - Time, text and random helpers (Priority: P2)

**Goal:** `std.time`, `std.text`, `std.random` (FR-007–FR-009).

**Independent test:** `python3 tests/time_check.py` and `tests/run.sh tests/cases/time tests/cases/text tests/cases/random` pass, repeated 3 times byte-identically.

- [ ] T024 [P] [US4] Golden cases: `tests/cases/time/` (epoch, leap days 2000/1900/2024, year 1 and 9999, negative times, month ends, `format` with and without ms, `parse` with `Z`/offsets/date only, malformed text, `now` with `--clock-ms`, `from_ms` out of range: R0007), `tests/cases/text/` (each helper on empty strings, multi-byte text, `replace` with overlapping matches, `pad_left` with a multi-byte fill, `lines` with `\r\n`, `words` with leading/trailing whitespace, the documented R0007s), `tests/cases/random/` (`unit`, `pick`, `shuffle` under `--seed 1` and `--seed 2`; `pick` of an empty list; a function calling `pick` without `uses random`: E0501 with the fix).
- [X] T025 [P] [US4] `std/time.vo` per contracts/library.md (Hinnant's days_from_civil / civil_from_days; ISO weekday; ISO 8601 parse and format; `now` = `now_ms()`), with doc comments.
- [X] T026 [P] [US4] `std/text.vo` (complete it: replace, contains, repeat, pad_left, pad_right, lines, words) and `std/random.vo` (unit, pick, shuffle) per contracts/library.md, with doc comments.
- [ ] T027 [US4] `tests/time_check.py`: 10,000 times (seeded) across years 1–9999 and around 1970, converted by a vo driver (`from_ms`, `format`, `parse`, `to_ms`) and compared with Python's `datetime` (proleptic Gregorian, UTC); add to the `Makefile` `test` list.

**Checkpoint:** the helpers work and are deterministic.

---

## Phase 7: User Story 5 - Plain HTTP behind `net` (Priority: P2)

**Goal:** `std.http` over `__http_exchange` (FR-010, FR-011, research R9–R11).

**Independent test:** `python3 tests/steps.py tests/cases/http` passes (server started by the runner), repeated 3 times.

- [ ] T028 [P] [US5] `tests/http_server.py` (fixed routes on a given port: `/plain`, `/json`, `/chunked`, `/status/404`, `/status/500`, `/redirect` (302 + Location), `/slow` (sleeps longer than a 1 s timeout), `/large` (17 MiB), `/malformed`, `/echo` (POST: returns the body and content type)); `tests/steps.py` gains `server NAME` (start it on a free port, substitute `{port}` in later lines, stop it at the end) and `stop-server`.
- [ ] T029 [P] [US5] Step cases in `tests/cases/http/`: `get_plain`, `get_json` (with `std.json`), `chunked`, `status_404`, `redirect_not_followed`, `post_echo`, `header_lookup` (case-insensitive), `https_refused`, `bad_url`, `refused` (no server), `unknown_host`, `slow` (`--timeout 1`: R0009), `too_large`, `malformed`, `scope_denied` (`uses net("example.invalid")` requesting localhost: R0010), `missing_effect` (E0501 with the fix), `record_replay` (record with the server, stop it, replay: identical output; replay requesting another port: R0014).
- [ ] T030 [US5] `src/net.s`: `__http_exchange(host, port, request) -> result[str]`: `rt_check_name(EF_NET, host)` against the frame's grant; resolve (IPv4 literal, `/etc/hosts`, one UDP DNS A query to the first `nameserver` of `/etc/resolv.conf`, retried until the deadline); non-blocking `connect` + `ppoll` against the `--timeout` deadline; send all; read until close, `err` past 16 MiB; every failure an `err` with a message. Raw syscalls added to `src/sys.s` (`socket`, `connect`, `sendto`, `recvfrom`, `setsockopt` if needed); `btab` entry in `src/rt_call.s`. Speed check.
- [ ] T031 [US5] Record and replay (R11) in `src/replay.s`: the `io_http` wrapper for the stub (`EVK_HTTP` with host, port, result, value), replay matching host and port (R0014 otherwise), recording format documented in spec 006's `contracts/recording.md` (add the `http` kind there and in the reference).
- [ ] T032 [US5] `std/http.vo`: URL parsing (`http://` only; `https://` → `err "https is not supported"`), the request text, response parsing (status line, headers in order, `Content-Length`, chunked, to-close bodies), `get`, `post`, `header`, with doc comments. Accept T029's cases.

**Checkpoint:** HTTP works against the test server, under scopes, and replays.

---

## Phase 8: User Story 6 - Evidence (Priority: P2)

**Goal:** SC-001, SC-002.

**Independent test:** `python3 tests/stdlib_eval.py` and `python3 tests/doc_conformance.py` exit 0.

- [ ] T033 [P] [US6] Write ≥ 10 task statements in `tests/stdlib_eval/tNN-*/task.md` with visible test inputs and expected outputs (JSON round trip, config file update, JSON report from CSV-like input, date arithmetic and weekday report, ISO timestamps between time zones, a padded text table, word frequency with `words`, a seeded shuffle and pick, an HTTP client against the test server's `/json`, a program that uses a package from a manifest in the task directory). Write them before any solution exists.
- [ ] T034 [US6] Solve each task with `claude -p` run with no tools, an empty working directory and no user settings (memory: benchmark agent isolation), given the task statement, `docs/reference.md` and the full `vo doc` text output, and nothing from `std/` or `tests/`. A harness (`tests/stdlib_eval.py --solve`) gives it up to 3 rounds: it writes `main.vo`, the harness runs `vo check` and the visible tests and feeds the output back. Save the solutions as `tests/stdlib_eval/tNN-*/main.vo` and record the rounds per task in `results.md`.
- [ ] T035 [US6] `tests/stdlib_eval.py` (runs each solution on its inputs, starting the HTTP test server when the task needs it) and a `stdlib-eval` Makefile target; run it and record the table in `results.md` (SC-001). Record `tests/doc_conformance.py`'s counts (items, wrong-type and wrong-count programs, all rejected) for SC-002.

**Checkpoint:** all stories complete.

---

## Phase 9: Polish & Cross-Cutting Concerns

- [ ] T036 [P] `docs/reference.md`: a section "Standard library and packages" (the five modules in one line each, `import std.json.parse_json;`, `vo doc`, `vo.packages` and `vo package`, the effects rule for library calls, the E07xx codes), with executed examples (a `std.text`/`std.json` program and a `console` block for `vo doc std.text.pad_left`); the `net` row of the effects table names `std.http`; Command line lists `doc` and `package`; record the token growth (≤ 2,000) in `results.md`.
- [X] T037 [P] `docs/errors.md`: confirm entries for E0701–E0705 and T0010; extend E0201, E0205, E0501 and R0007's text for their new situations (contracts/codes.md); add spec 007's `contracts/codes.md` to `tests/catalog.py`'s sources; `python3 tests/catalog.py` passes.
- [ ] T038 [P] Fuzzing: `tests/fuzz_inputs.py` gains mutated `vo.packages` manifests and mutated HTTP responses served by the test server (never exit 125 or a signal).
- [ ] T039 Run the full gate: `make test`, `make gc-check` (step cases under the stress build, including HTTP), `make speed-check`, `python3 tests/speed.py --baseline build-base/vo` (SC-007), the 703 bench2 runs, `make lint`, `make stdlib-eval`. Record every result in `results.md`.
- [ ] T040 Update `README.md` and `docs/roadmap.md` (007 done; follow-ups: the agent run with and without `vo doc`, HTTPS, downloading packages), and finish `results.md` (SC table, deviations, `ponytail:` markers, follow-ups).

---

## Dependencies & Execution Order

- **Setup (Phase 1)** → **Foundational (Phase 2)** → stories.
- **US1** needs Phase 2 (T005's `std/text.vo` skeleton gives it items to document); its package listing (T023) waits for US3.
- **US2** needs Phase 2; its JSON is used by US5's `get_json` case and US6's tasks.
- **US3** needs Phase 2 only.
- **US4** needs Phase 2 (T006's effect rule for `random` and `clock`).
- **US5** needs Phase 2 and US2 (tests), and spec 006's replay code.
- **US6** needs US1–US5.
- **Polish** after the stories it documents.

### Parallel opportunities

- T002, T003 after T001.
- After Phase 2: US2 (std/json.vo, num.s), US3 (sha256.s, packages.s), US4 (std/time.vo, std/text.vo, std/random.vo) touch different files and can proceed in parallel; `src/main.s` (doc, package commands) and `src/loader.s` (std and package roots) are shared: do those edits in sequence.
- Test-writing tasks marked [P] in every story can be written up front.
- T036, T037, T038 in parallel.

## Parallel Example: after Phase 2

```text
Track A: T009 → T010 → T011 → T012 (US1)
Track B: T013, T014 → T015 → T016 → T017 (US2)
Track C: T018, T019 → T020 → T021 → T022 → T023 (US3)
Track D: T024 → T025, T026 → T027 (US4)
```

## Implementation Strategy

1. Phases 1–2: the embedded library, its imports and its effect rule.
2. US1 → `vo doc` (MVP: agents can look signatures up).
3. US2 → JSON; US3 → packages (in either order).
4. US4, US5 → the remaining modules.
5. US6 → the evidence; Polish → reference, catalog, fuzzing, gate, results.
