# Implementation Plan: Small Language and Toolchain Fixes

**Branch**: `009-small-fixes` | **Date**: 2026-11-15 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/009-small-fixes/spec.md`

## Summary

Five small fixes found while writing the example applications (research R1-R6):

1. **`get_or(m, key, default)`**: builtin id 55, typed like `has`, pure; it never inserts.
2. **Replay matches the request line**: the `http` recording event gains `"request":"GET /path"`;
   replay compares it (R0014 on a difference); events without it match by host and port as before.
3. **R0010 names the granting function**: `rt_denied` walks up through frames that inherit their caller's
   grant (library and anonymous functions) to the function whose `uses` clause was checked.
4. **`case ok()`**: parsed as an empty-parentheses arm; accepted for `result[void]`; E0411 with a fix
   elsewhere; W0402 with a fix for `case ok(_)` / `case ok` on a `result[void]`.
5. **`align_left` / `align_right`** in `std/text.vo`.

## Technical Context

**Language/Version**: C (gnu11, freestanding; spec 008) for the toolchain; vo for `std/text.vo`.

**Primary Dependencies**: none at run time; GCC for x86-64, ARM64 and RISC-V; qemu-user for the foreign
builds; Python 3 for tests.

**Storage**: recordings (`vo run --record`): one added field in `http` events.

**Testing**: golden cases (`tests/cases/{run,check,http,text}`), HTTP step cases (`tests/steps.py`
with the test server), `tests/catalog.py` (new codes), `tests/ref_examples.py`, `tests/doc_conformance.py`
(the new builtin row and library items), the full `make gate` and the other platforms' suites.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: no change: `get_or` is a builtin call like `has`; nothing else is on a hot path.

**Constraints**: Principle I (one form; no overloading or default arguments) shaped fixes 4 and 5;
existing programs behave as before except the four recorded exceptions (spec FR-010); the reference grows
by at most 300 tokens.

**Scale/Scope**: about 150 lines of C, 20 of vo, 15 golden or step cases, 2 catalog entries.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** One form for `result[void]` success (`case ok()`), reached through a warning with a fix; padding with spaces gets two named functions instead of a default argument; no overloading. |
| II. Machine-readable feedback | **PASS.** E0411 and W0402 carry fixes; R0010's JSON report names the same function as its message; R0014 names both requests. No field removed (recordings stay schema 1, one field added). |
| III. Determinism | **PASS.** `get_or` is pure; replay becomes stricter, never looser. |
| IV. Test-first through the CLI | **PASS.** Each fix starts with golden or step cases that fail on the current binary; both new codes get a catalog entry and a case. |
| V. Single source of truth, minimal surface | **PASS.** One builtin and two library functions; documentation in the reference table and `vo doc`. |
| Implementation constraints | **PASS.** C with zero warnings on all three architectures; no assembly change. |

## Project Structure

### Documentation (this feature)

```text
specs/009-small-fixes/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/language.md   # the five behaviors, codes E0411 and W0402, the recording field
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
src/check_expr.c     # get_or: name, typing (B_GET_OR 55, B_COUNT)
src/rt_call.c        # get_or at run time (BU_GET_OR)
src/vo.h             # B_LAST 55; NF_EMPTY_ARM; EV_REQUEST, EV_SIZE 56
src/parse.c          # `case NAME()` arms
src/check.c          # E0411, W0402 in check_arm
src/rt_effects.c     # rt_denied names the grant frame
src/replay.c         # the request line: record, read, match (R0014)
std/text.vo          # align_left, align_right
docs/reference.md    # builtins row, case ok(), replay matching
docs/errors.md       # E0411, W0402
tests/gen_programs.py  # helper renamed to lookup_or
tests/cases/...      # golden and step cases
examples/            # get_or and align_* where they fit (SC-001, SC-007)
```

**Structure Decision**: no new files in `src/`; each fix lives where its mechanism already is.

## Complexity Tracking

No violations to justify.
