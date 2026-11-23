# Implementation Plan: Recorded Tests

**Branch**: `054-recorded-tests` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/054-recorded-tests/spec.md`

## Summary

`vo test` gains `--record DIR`, `--replay DIR` and `--redact NAME`. The recording machinery of `vo run
--record` (`src/replay.c`) is reused per test: the runner opens a recording before each test and
closes it after (or loads the test's file before it and checks for unused inputs after), since it
already gives each test its own heap, recovery point and deadline (R1). A mismatch is the existing
R0014, raised in the test, so it fails that test only; a missing recording is R0014 with its own
message (R2). Test recordings add the request's headers and body to HTTP events, compare the body in
replay, and redact credentials and named values at record time (R3, R4). File names come from the
test identifier by a reversible rule (R5). A new example calls a public API and commits its
recording (R8).

| Part | File | What |
|---|---|---|
| Options | `src/main.c` | `--record DIR` and `--replay DIR` for `test`, `--redact NAME`, exclusions (T0001) |
| Per-test recording | `src/replay.c`, `src/testrun.c` | start/finish/load per test; test header; file names; unused events and changed-code notes; unused files; R0014 messages for tests |
| HTTP detail and redaction | `src/replay.c` (and the HTTP wrappers it calls) | `sent` headers and body; body comparison; redaction of fixed and named values |
| Results | `src/testrun.c` | `(recorded)`/`(replayed)`, notes, JSON fields, summary |
| Docs | `docs/reference.md`, `docs/errors.md` (R0014 text), `site/docs/guide/tests.md`, roadmap, changelog | R8b |
| Example | `examples/rates/` (new), `Makefile` (example loop replays `recordings/tests`) | R8 |
| Tests | `tests/cases/recorded_tests_steps/` (new step cases), `tests/recorded_tests.py` (new), `tools/quick.sh` | R9 |

## Technical Context

**Language/Version**: C11 freestanding (toolchain); vo (example); Python 3 (test drivers, local servers).

**Primary Dependencies**: none new.

**Storage**: recording files (JSON lines) in the directory given.

**Testing**: step cases with a local HTTP server; `tests/recorded_tests.py`; `make quick AREA=recorded` (a new area: `recorded_tests_steps`, `replay`, `test_steps`, `test`);
`make test` once at the end; the instrumented and other-architecture builds.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain (CLI change; no language change).

**Performance Goals**: SC-003 (replay under 1% of a 100 ms-latency live run): replay reads a small file
and touches no network.

**Constraints**: recording allocates nothing while writing (as today); no static buffer over 64 KiB;
`vo run --record`/`vo replay` byte-identical; reference ≤ 15,000 tokens (~167 left).

**Scale/Scope**: about 300 lines of C in `src/replay.c` and `src/testrun.c`, 40 in `src/main.c`, 15 step
cases, 300 lines of Python, one example.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way: two options on the command agents already run; no syntax; mocks stay out. | Pass |
| II. Machine-readable feedback | Mismatches are R0014 (stable) with test-specific text; JSON gains fields only. | Pass |
| III. Determinism | Replayed tests feed exactly recorded inputs; seed and clock already fixed; files have no times. | Pass |
| IV. Test-first | Step cases of each story before the code. | Pass |
| V. Minimal surface | Reuses the recording schema and `rp_take`; ~60 reference tokens, stated. | Pass |
| VI. Backend network services | Write: tests of code that calls services without live access or fakes. Review: the request and the answer it relies on are a diffable file. Operate: CI runs offline. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/054-recorded-tests/
├── plan.md
├── research.md          # R1–R9
├── data-model.md        # test recording, directory, redaction, JSON fields
├── quickstart.md
├── contracts/
│   └── cli.md           # options, file names, results, recording additions
└── tasks.md             # /speckit-tasks
```

**Structure Decision**: no new C file; the per-test parts sit beside the code they extend.

## Complexity Tracking

No violations.
