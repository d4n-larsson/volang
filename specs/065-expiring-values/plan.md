# Implementation Plan: Expiring values

**Branch**: `065-expiring-values` (work on `main`, as for specs 001–064) | **Date**: 2026-11-28 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/065-expiring-values/spec.md`

## Summary

A value that is only good for a while (a token, a cached response, a reading, a lease) is wrapped in
`time.Expiring[T]`. The only way to the value is `e.value_at(now)`, which returns a result, so vo's
existing rule that a result must be handled forces every reader to deal with expiry.

The whole feature is about 30 lines of vo in `std/time.vo`: one record and three functions. The value
is hidden by capturing it in a closure, which a prototype run confirmed the checker already guards in
every way the spec lists (research R1). The toolchain's C code does not change, and no code is added
to the catalog.

| Part | File | What |
|---|---|---|
| The type | `std/time.vo` | `Expiring[T]`, `expiring`, `expiring_for`, `left_ms`, each with an executed example |
| Tests | `tests/cases/time/`, `tests/cases/replay/` | four golden cases (R9), written first |
| Docs | `docs/reference.md` (about 14 words in the `std.time` row, paid for by a cut of the same size), `site/docs/guide/standard-library.md`, `CHANGELOG.md`, `docs/roadmap.md` | R9 |

## Technical Context

**Language/Version**: vo (standard library code). No C.

**Primary Dependencies**: none new. Uses closures, generic records and `format`, all existing.

**Storage**: none.

**Testing**: golden cases in `tests/cases/time/` and one under `tests/cases/replay/`; the comment
examples through `tests/doc_examples.py`; the guide example through `tests/site_examples.py`;
`tests/std_names.py` for name clashes. `make quick AREA=time` between edits, `make test` once per
change; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (no platform-specific code; the suite runs on each
in the gate).

**Project Type**: compiler toolchain with an embedded standard library.

**Performance Goals**: none. A read is one closure call and one comparison.

**Constraints**: `git diff src/` empty (SC-005). Reference ≤ 15,000 tokens by the suite's count (words
× 1.3). Spec 064's plan measured it at 14,998, so the row's roughly 18 tokens need a cut of the same
size elsewhere in the reference, stated in the change. Spec 064 is drafted and not implemented; it
needs 350 tokens of cuts of its own, and whichever lands second re-measures.

**Scale/Scope**: about 30 lines of vo, 4 golden cases, about 40 lines of guide text.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | No syntax, keyword or builtin. One way to make from a moment, one from a duration, one way to read (the field call; no function beside it, R2). The kind of mistake it removes, using a value without looking at its expiry, becomes a check error with an existing code and, for a dropped result, a mechanical fix. | Pass |
| II. Machine-readable feedback | No new code or schema field. Every misuse is reported by E0301, E0308, E0328 or E0404, each already in the catalog with a golden case; this spec adds a case per misuse of the new type. | Pass |
| III. Determinism | No function added reads the clock or has an effect: the same value and time give the same result. Overflow of time plus duration is R0001, the language's defined behavior. Recordings replay as before (R7). | Pass |
| IV. Test-first | The four golden cases and the comment examples are written first and fail (the names do not exist). No new code, so no catalog coverage is owed. | Pass |
| V. Single source of truth, minimal surface | Reference row, guide, changelog and roadmap change with the code; the reference's growth is stated and paid for. Three functions; left out by the spec: a cache, automatic refresh, leases, sliding expiry, timers, a stored form, use inside `std.security`. | Pass |
| VI. Backend network services | Write: an agent that holds another system's token cannot forget that it expires. Review: `token: time.Expiring[str]` in a signature says what the function received, and the reading line shows which time it was read at. Operate: the error text gives both times for the log. | Pass |
| Vendored code, assembly, C rules | None touched. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/065-expiring-values/
├── plan.md
├── research.md          # R1–R9
├── data-model.md
├── quickstart.md
├── contracts/
│   └── std-time.md      # the four items, the misuse table, the reference row
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/time.vo
tests/cases/time/expiring.{vo,out,code}
tests/cases/time/expiring_misuse.{vo,out,code}
tests/cases/time/expiring_overflow.{vo,out,code}
tests/cases/replay/   (one recorded run, in that directory's layout)
docs/reference.md, docs/roadmap.md, CHANGELOG.md
site/docs/guide/standard-library.md
```

**Structure Decision**: the items go into the existing `std/time.vo`. A module of its own would cost a
row in the reference and entries in `Makefile` and `src/doc.c` (R4).

## Order of work

1. The golden cases and the comment examples, failing: `expiring`, `expiring_misuse` (confirm here the
   exact report for a value sent to a worker; the prototype did not reach it), `expiring_overflow`, the
   replay case.
2. The record and the three functions in `std/time.vo`; `make quick AREA=time`.
3. The reference row and its cut; the guide's token example and the store-and-rebuild pattern;
   changelog and roadmap.
4. `make test` once. `git diff --stat src/` is empty.
5. SC-002's five agent runs only when the user says to run them.

## Complexity Tracking

No violations.

Decisions that narrow or read the spec, for the record:

| Decision | Why |
|---|---|
| The error is a `str`, not a record with the two times (R3) | FR-004 asks that it carry both times, have one text form and pass on with `try`. A record error fails the last for every function that returns `result[T]`. The text carries both times. |
| The expiry moment is the field `expires_at`; the read is the field call `value_at` (R2) | FR-006 and FR-003 ask for the ability, not for functions; a function beside each field would be a second spelling. |
| An expiring value cannot be compared, turned into JSON or sent to a worker (R1) | FR-010 allows a check error for each. It follows from hiding the value in a function, which is what lets FR-003 hold with no checker change. |
| A hand-built `Expiring` may carry an `expires_at` that disagrees with its `value_at` (R5) | The value stays guarded by the true moment. Closing this needs private fields, which FR-007 rules out. |
