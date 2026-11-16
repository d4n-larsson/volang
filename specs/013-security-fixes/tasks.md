---

description: "Task list for vo-lang spec 013: security fixes from review 1"
---

# Tasks: Security Fixes from Review 1

**Input**: Design documents from `specs/013-security-fixes/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R7), data-model.md, contracts/behavior.md,
contracts/codes.md, quickstart.md; the findings in docs/security-review-1.md

**Tests**: REQUIRED (FR-026): each finding's reproduction becomes a case that fails before its fix.

**Organization**: one phase per user story (US1-US4 P1, US5-US6 P2), then the ledger and the gate.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- Cases for findings live in `tests/cases/security/` (golden and step cases, named after the finding:
  `c1_nul_path.vo`, ...); race, rename and server-attack scripts in `tests/security/`, run by
  `make security-check`.
- **Regression rule:** honest programs keep byte-identical results; `make test` after every task.

---

## Phase 1: Setup

- [X] T001 `specs/013-security-fixes/results.md` (SC table; a row per finding: case, status); add
  `013-security-fixes` to `tests/catalog.py`; `tests/security/` and a `security-check` target in the
  Makefile.

---

## Phase 2: User Story 1 - The grant holds (Priority: P1)

- [X] T002 [P] [US1] Cases from the reproductions: C2 (list, map, result, generic record, generic variant,
  bound effect parameter, contract), C3 (the `each` pattern for file.read, file.write, env, net, serve),
  H1 (sub-scope through a link: step case creating the link), C1 (NUL in read and write paths), H2 (start
  directory over 4096 bytes: step case), L1 (literal outside the scope). Confirm they fail now.
- [X] T003 [US1] Root grant (research R1): `src/rt_effects.c` checks the root grant in `rt_check_bit`,
  `rt_check_name`, `rt_check_path`; the root is set in `src/interp.c` (run), `src/testrun.c` (tests),
  `src/prop.c` (properties), `src/replay.c` (replay); `src/vm.c`: an effect parameter's part of a generic
  frame's grant comes from the caller.
- [X] T004 [US1] Nested function types (research R2): `src/check_generic.c` checks every argument and
  generic variant payload against the instantiated type; `src/types.c` `ty_unify` compares function
  effects; `src/check_effects.c` L1 literal targets (E0503).
- [X] T005 [US1] NUL paths refused; the start directory fails closed (`src/rt_effects.c`).

---

## Phase 3: User Story 2 - No redirection (Priority: P1)

- [X] T006 [P] [US2] `tests/security/race.sh` (H4) and `rename.sh` (H3) from the review's scripts,
  counting accesses outside the scope over N runs; plus a step case for an inside link that keeps working.
- [X] T007 [US2] Start descriptor and scoped `openat2(RESOLVE_BENEATH)` (research R3): `src/platform/*.h`
  (openat2, getrandom), `src/sys.c`, `src/rt_io.c`, `src/rt_effects.c`; unscoped opens from the start
  descriptor.

---

## Phase 4: User Story 3 - No corruption or crash from vo code (Priority: P1)

- [X] T008 [P] [US3] Cases: C4 (`sort_by` shrink: R0007), H11 (cyclic and 3,000,000-deep `==`: R0005 or a
  result), L10 (1,000,000-deep `expect_eq` rendering: R0005). Confirm they fail now.
- [X] T009 [US3] `src/rt_func.c` snapshot; `src/interp.c` `values_equal` and `src/rt_render.c`
  `render_value` stack checks and identity shortcut; `src/gc.c` root-stack overflow while running: R0005.

---

## Phase 5: User Story 4 - A server survives any request (Priority: P1)

- [X] T010 [P] [US4] `tests/security/server_attacks.py` (the review's H7, H8, H9, H10, L11, M5 attacks
  against one server, checking a normal request after each and memory under the limit) and step cases
  for H7 and M5.
- [X] T011 [US4] One C framing for requests and responses (research R4) in `src/serve.c` and
  `src/net.c`; ASCII heads; decoded bodies; the total cap; deadline and stop signal on every read;
  incremental scanning (L9); `std/http.vo`: parsing inside the guard, no `dechunk`, header checks (M5).
- [X] T012 [US4] Server state (H10, L11): no timeout inside the collector (`gc_busy`); restore `gc_on`, the
  out-of-memory hook and error extras in `rt_serve_call` and `pr_try`.

---

## Phase 6: User Story 5 - Hostile servers, recordings and packages (Priority: P2)

- [X] T013 [P] [US5] Cases: M2 (hostile responses to `get`), C5 (recording program paths: absolute, `..`,
  non-`.vo`, FIFO), M1 and L2 (forged `ok`; no file system access), L6 (bad literals, overflow, size), H5
  (symlinked and unreadable package subdirectories), H6 (hash/load bytes), L3 (NUL in a manifest token).
- [X] T014 [US5] `src/replay.c`: program path rules, lexical file checks, strict reader and size cap.
- [X] T015 [US5] `src/packages.c`, `src/main.c` (`walk_dir`), `src/loader.c`: contained directory links,
  unreadable directories refuse the package, files loaded from the hashed bytes, manifest control bytes.

---

## Phase 7: User Story 6 - Tools on hostile trees (Priority: P2)

- [X] T016 [P] [US6] Cases: M3 (planted temporary-file link for fmt, edit, effects --update, package), M4
  (import link outside: E0211; inside: works), L4 (FIFO `*.vo`), L7 (escape bytes in a diagnostic).
- [X] T017 [US6] `src/main.c` and `src/edit.c` exclusive temporary files; `src/loader.c` import
  containment (E0211); `src/sys.c` regular files only; `src/diag.c` escaped control bytes; `src/packages.c`
  L5 (E0701's command); `src/net.c` L8 (random DNS ID, question match); `docs/errors.md` E0211.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T018 Docs: the reference's root-grant rule (≤ 150 tokens); `docs/security.md` (system calls, kernel
  5.6, links, limits); `docs/security-review-1.md` status column (fixed, with the case).
- [X] T019 `make security-check` passes (race and rename 1,000 times each: 0 escapes; every server attack
  survived; the review's programs re-run: 0 findings reproduced); the full gate on three platforms,
  `make instrumented`, `make diff-asm COUNT=10000`, the evaluation suites; fill `results.md`.

---

## Dependencies & Execution Order

- Setup → US1 (the root grant first: it bounds everything else) → US2-US6 in any order → Polish.
- Within a story: cases first, then the fix.

## Implementation Strategy

MVP: US1 and US2 (the boundary), then the runtime and server fixes, then replay, packages and tools.
