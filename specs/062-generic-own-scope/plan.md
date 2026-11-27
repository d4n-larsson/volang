# Implementation Plan: A Generic Function's Own Scope

**Branch**: `062-generic-own-scope` | **Date**: 2026-11-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/062-generic-own-scope/spec.md`

## Summary

A function with an effect parameter runs today with the union of its declaration and its caller's grant,
and the union loses the function's own scopes. The fix changes what the run-time scope check walks, not
what a frame is granted:

1. **The walk reads each function's own declaration** (the scopes written in its `uses` clause), not the
   frame's merged set. The merged set stays, for the question "does this code have the effect at all".
2. **A call of a function value typed with effect parameters only is transparent**: walking outward
   through it, the calling function's own scopes are not applied, and the walk remembers that what it is
   checking runs as an effect parameter. A generic function called by name that was handed the parameter
   on is then transparent too (research R2, corrected during implementation).
3. **Such a call written inside an anonymous function exempts the activation that created the anonymous
   function**, found by an exact identity (a serial), so that a function's own wrapper around a callback
   does not narrow the callback.

Nothing is added to the frame, the checker accepts the same programs, and calls that involve no generic
function with effect parameters cost what they cost today. Details: [research.md](research.md); the rule
as a contract: [contracts/call-path.md](contracts/call-path.md).

## Technical Context

**Language/Version**: C (gnu11, freestanding, no libc), one platform file per architecture

**Primary Dependencies**: none new

**Storage**: N/A

**Testing**: golden cases (`tests/run.sh`), step cases (`tests/steps.py`), `tests/scope_fuzz.py` and
`tests/scope_paths.py` (extended), `tests/security/*.py`, the stress build (`make gc-check`)

**Target Platform**: Linux on x86-64, ARM64 and RISC-V (the last two under qemu in the gate)

**Project Type**: compiler and run time (single binary)

**Performance Goals**: `tests/speed.py` within the gate's limits (RISC-V at most 10% over the baseline);
no added work on a call that is not a generic call with effect parameters or a call of a closure created
under one; a scoped operation stays linear in the call depth

**Constraints**: the frame header stays 64 bytes (`FR_SLOTS`); `Node.flags` is 16 bits and all 16 are in
use, so the new mark reuses a bit that cannot occur on a call node (R3); the collector must see anything
new that points at heap objects (nothing does: the table holds frame addresses and integers)

**Scale/Scope**: about 150 lines in `src/rt_effects.c`, `src/vm.c`, `src/interp.c`, `src/check_effects.c`
and `src/vo.h`; about 25 golden cases; one fuzzer extension; four documents

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | No syntax, no new names. A declared scope means the same on a generic function as on any other, which removes a rule an agent would have to know. | Pass |
| II. Machine-readable feedback | R0010 keeps its code, text form and JSON fields; `declared_by` names the generic function and `allowed` its own scopes. No new code. | Pass |
| III. Determinism | The decision uses the call path and the checker's types only; serials are counted per process and never printed. Same result when recording, replaying and on three architectures (FR-010). | Pass |
| IV. Test-first | Every acceptance scenario is a golden case written before the code and seen to fail (story 1) or pass (story 2) on 0.7.0, as measured in the spec. | Pass |
| V. Minimal surface | One paragraph changes in `docs/security.md` (S2) and one sentence in the reference; the token budget is checked. One deliberate shortcut, marked in the code: the activation table is a fixed array (R5). | Pass |
| VI. Backend network services | Review: a scope on a helper is a guarantee about the helper. Operate: a refusal names whose declaration refused. Write: callbacks, handlers and pooled connections keep what the caller granted. | Pass |

Re-checked after Phase 1: unchanged. One point needs the user's eye, recorded in research R6: programs
that relied on the leak stop with R0010. That is the purpose of the change and is allowed for a security
fix before 1.0, with a changelog entry (spec, Assumptions).

## Project Structure

### Documentation (this feature)

```text
specs/062-generic-own-scope/
├── spec.md
├── plan.md              # this file
├── research.md          # R1 to R8
├── data-model.md        # the mark, the closure's owner, the activation table, the walk
├── quickstart.md        # how to see it work
├── contracts/
│   └── call-path.md     # the rule, R0010's output, the cases
├── checklists/
│   └── requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/
├── vo.h                 # NF_FX_PARAM (a reused flag bit on call nodes), Closure.owner, CL_OWNER
├── check_effects.c      # marks a call whose effects are effect parameters only
├── rt_effects.c         # the walk: own declarations, transparent calls, exempt activations
├── vm.c                 # vm_push and the inlined generic entry: register an activation
├── interp.c             # make_closure: the owner's serial
└── types.c              # rt_es_grant: comment only (the union is for bits, not scopes)

tests/
├── cases/effect_sets/   # generic_own_*.vo: stories 1 to 3 for file, net, env, process, serve
├── cases/security/      # generic_scope_*.vo: the leaks ruled out in research R4
├── scope_fuzz.py        # generic helpers in the generated programs, checked against its model
└── security/            # nested_race.py gains a generic helper variant

docs/
├── security.md          # S2 states the rule
├── reference.md         # one sentence where effect parameters are described
├── bug-sweep-2026-11-27.md   # finding 12: fixed
└── CHANGELOG.md (root)  # the entry of FR-012
```

**Structure Decision**: the existing single-binary layout. The whole decision is in one function of
`src/rt_effects.c` (the iterator that replaces `path_scoped`); the other files only supply what it reads.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| An activation table beside the frame stack | The exemption of rule 3 must name one activation exactly; an address alone can be a later activation of the same function (R4, case L3) | A field in the frame grows every frame of every program by 8 bytes and touches the three platform files' frame constants; matching by function alone leaks (R4) |
| A fixed limit on that table and on pending exemptions | Freestanding C, no allocation on the call path | A growing table needs the collector's cooperation; past the limit the walk applies every scope, which refuses more and never less |
