# Implementation Plan: Scoped Effects Through Package Functions

**Branch**: `050-scoped-package-effects` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/050-scoped-package-effects/spec.md`

## Summary

Two small changes that mirror what the toolchain already does for the standard library.

**Checker** (R1): when a callee's declaration enters its caller's requirement, each effect it declares
without scopes (of the six that take scopes) is recorded as "needed in any scope" instead of "needed
whole". That is the form a standard-library call already has, so the covering test (`es_missing`),
E0501, E0503, W0501 and their fixes work unchanged. The effects record does not go through this path
and keeps reporting `net("a")` to `net` as growth.

**Run time** (R2): the scope checks compare the target with every distinct scoped set on the frame
chain instead of two sets (the running frame's and the root's). Frames are not changed and calls cost
what they cost today; an unscoped helper's frame passes and the walk goes on to its callers.

| Part | File | What |
|---|---|---|
| Requirement form | `src/effects.c`, `src/vo.h` | `es_requirement(set)`: unscoped scopeable bits moved from `mask` to `any` |
| Checker | `src/check_effects.c` | applied in `fx_value_call`, the edge fixpoint and `fx_related` |
| Run time | `src/rt_effects.c` | an iterator over the scoped sets of the call path; `rt_check_name`, `rt_check_net`, `rt_check_path`, `rt_bit_scoped`, `rt_path_allowed`, `rt_open_parent`, `rt_open_file` use it |
| Catalog | `docs/errors.md` (E0503, R0010) | the rule; the E0503 example stays valid |
| Reference | `docs/reference.md` | two sentences reworded, net zero words (R6) |
| Docs | `docs/security.md` (S1, S2, limits), `site/docs/guide/effects.md`, `site/docs/guide/modules-and-packages.md`, `docs/roadmap.md`, `CHANGELOG.md`, `specs/049-postgres-package/` (prerequisite met) | R7 |
| Tests | `tests/cases/effects/` (new check and run cases), `tests/scope_paths.py` (new, SC-002), `tests/scope_fuzz.py` (a helper variant), `tools/quick.sh` | R5 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc); Python 3 for test drivers.

**Primary Dependencies**: none new.

**Storage**: none.

**Testing**: golden cases in `tests/cases/effects/` first (they fail before the change);
`tests/scope_paths.py` generates programs and predicts each outcome from a model of the rule;
`make quick AREA=effects` between edits, `make test` once per change, the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain.

**Performance Goals**: SC-005. Calls are untouched. A scoped check walks the frame chain: one pointer
comparison per frame, one scope comparison per distinct scoped set.

**Constraints**: the reference has 1 token of budget left (`tests/ref_examples.py`: about 14,999 of
15,000), so its two sentences are reworded within the words they have now (R6); no new syntax, code
or JSON field; zero warnings; the instrumented build passes.

**Scale/Scope**: about 25 lines in `src/effects.c`, 15 in `src/check_effects.c`, 90 changed in
`src/rt_effects.c`, 300 of Python, about 25 new cases.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | No new form. An agent writes a helper with the plain effect, as it already does; the checker stops offering "drop your scopes" as a fix. One rule for program, module, package and function value. | Pass |
| II. Machine-readable feedback | No new code; E0503 and R0010 keep their fields and meaning; catalog entries updated in the same change. | Pass |
| III. Determinism | The run-time check is a pure function of the call path and the target; replay runs the same checks. | Pass |
| IV. Test-first | New golden cases (check and run) are written first and fail on today's build: the probes of spec 049 R1 show they do. | Pass |
| V. Single source of truth, minimal surface | Reference reworded at net zero; catalog and security document updated. One known ceiling gets a `ponytail:` comment (R2: the walk is linear in call depth). | Pass |
| Implementation constraints | C only, no assembly touched, no vendored code. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/050-scoped-package-effects/
├── plan.md
├── research.md          # R1–R8
├── data-model.md        # effect set forms, the call path
├── quickstart.md
├── contracts/
│   └── scopes.md        # the rule at check time and run time, diagnostics
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/effects.c            # es_requirement
src/check_effects.c      # three call sites
src/rt_effects.c         # the path iterator and its seven users
src/vo.h                 # one prototype
docs/errors.md  docs/reference.md  docs/security.md
tests/cases/effects/     # helper_scoped_*, path_scope_*, value_scoped_*, fix_adds_scopes
tests/scope_paths.py
```

**Structure Decision**: no new source file; the change sits where scopes are already compared.

## Complexity Tracking

No violations.
