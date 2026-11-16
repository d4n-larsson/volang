# Implementation Plan: Complex Data Structures

**Branch**: `014-complex-data-structures` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/014-complex-data-structures/spec.md`

## Summary

Add an ordered map, an ordered set, a priority queue and a deque as built-in runtime types (C, like `list`
and `map`), reached through a new standard module `std.collections` (research R1-R4):
- **Types** (R2, revised with the user during implementation): four lowercase built-in types written and
  created like `list` and `map` (`new ordered_map[str, int]`), recognized only in type positions and
  after `new`; `ordered_map_by(cmp)` etc. in `std.collections` for another order. No new code.
- **Operations** (R4): the existing builtins (`len`, `has`, `remove`, `get_or`, `keys`, `push`, `pop`,
  indexing, `for`) extend to the new types; 24 library functions cover the rest, documented by `vo doc`.
- **Runtime** (R5): iterative AVL trees, a binary heap with insertion sequence numbers for ties, a ring
  buffer; equality, rendering and property generation by contents (R7).
- **Language** (R6, R8): records of ordered fields are ordered field by field; type arguments of `new` and
  of cases without a value are inferred from the expected type and the arguments.
- **Evaluation** (R9, R10): 8 benchmark tasks with two reference solutions each, a speed script, and the
  isolated agent protocol of spec 007.

## Technical Context

**Language/Version**: C (gnu11, freestanding) for the toolchain; vo for `std/collections.vo` (wrappers and
documentation).

**Primary Dependencies**: none new; GCC for three architectures; qemu-user 8.2; Python 3 for tests.

**Storage**: N/A.

**Testing**: golden cases for every operation and every new diagnostic (`tests/cases/collections/`,
`tests/cases/ordered/`, `tests/cases/generic/`); vo `prop` tests per structure against list models, run
under 10 seeds by `tests/structures_props.py` (SC-001); `tests/structures_bench.py` (SC-002/003);
`tests/structures_eval.py` (SC-004/005); the full gate on three platforms, instrumented, diff-asm 10,000.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-002 (100,000 operations under 5 s, linear-logarithmic scaling), SC-003 (within
5x of `map`); the speed gate stays under 1.10.

**Constraints**: Principle III (deterministic order; existing programs byte-identical); Principle II
(no new code; E0313 names the field); the reference grows by at most 300 tokens; library
documentation lives in `vo doc`, not the reference.

**Scale/Scope**: about 1,800 lines of C (runtime ~900, checker ~500, VM/compiler/render/prop ~400), 250
of vo, 4 type kinds, 1 new code, 24 library functions, 8 benchmark tasks.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No new syntax. One way to create each collection: `new` as for `list` and `map` (a comparison function: `_by`); lowercase names for built-in types, PascalCase for user types. On "no overloading": builtins have always been polymorphic over the built-in types (`len` and `slice` over `str`, `list`, `map`; `has`, `remove`, `keys` over `map`), and this spec extends that rule only: user and library functions stay unoverloadable (each `std.collections` name belongs to one type). Where an extended builtin's meaning depends on the type (`push` on a set adds; `pop` on a queue removes the smallest, on a deque the back), the reference row states the meaning per type (T020), so nothing is implicit. |
| II. Machine-readable feedback | **PASS.** No new code (a wrongly written collection is a syntax error); E0313's message names the field; collection values appear in JSON traces and locals through the existing rendered-literal form. |
| III. Determinism | **PASS.** No hashing or addresses in any order: trees by key, ties by insertion sequence, deques by position; iteration and rendering are defined; comparison functions are pure (effects refused). |
| IV. Test-first through the CLI | **PASS.** Golden cases and property tests land first and fail before each piece. |
| V. Single source of truth, minimal surface | **PASS.** Reference: one paragraph on ordered records and inference, the builtin rows gain the collection types, one line to `vo doc std.collections` (≤ 300 tokens). `contracts/` updated. Four structures are what the spec asks for; the loosely typed std-only primitive is a marked shortcut (Complexity Tracking). |
| Implementation constraints | **PASS.** C only, no assembly; iterative tree code (no native recursion over data); no new system calls; strict warnings and the instrumented build cover the new code. |

Re-checked after design (research, data model, contracts): no change.

## Project Structure

### Documentation (this feature)

```text
specs/014-complex-data-structures/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/api.md          # std.collections: types, constructors, builtins, library functions
├── contracts/language.md     # ordered records, inference
├── contracts/codes.md        # no new codes (read by tests/catalog.py)
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
src/rt_coll.c (new)          # AVL tree (map, set), heap, ring buffer; value_cmp order; snapshots
src/vo.h                     # TY_OMAP..TY_DEQUE (above TY_EFFSET: rt_concrete's <= TY_VOID shortcut);
                             # OBJ_COLL object, count at offset 8 (OPC_LEN); builtin id 61 (`__coll`)
src/check.c                  # resolve_type: the four std records -> kinds; ty_name_into; export_type
src/types.c                  # ty_has_params, ty_subst, ty_unify, memo for the new kinds
src/check_expr.c             # len/has/remove/get_or/keys/push/pop/indexing on the kinds; `==`; `new`;
                             # check_new_record inference (R8); the std-only `__coll` primitive
src/check.c (check_for)      # `for x in` set, queue, deque (map stays `for k in keys(m)`)
src/check_generic.c          # ty_ordered for records (R6), E0313 field message; deferred arguments (R8)
src/check_func.c             # sort_by key may be an ordered record
src/compile.c, src/vm.c      # `for` over a snapshot list; OPC_LEN works; op_cmp_param for records
src/rt_call.c, src/rt_list.c # builtin dispatch on the kinds; sort/min/max via value_cmp
src/interp.c, src/rt_render.c# values_equal and render_value by contents
src/prop.c                   # generate, copy and shrink collections through a list of elements
src/fmt.c, src/doc.c         # (fm_type unaffected: no new type syntax); doc_modules += collections
std/collections.vo (new)     # the library functions and `_by` constructors, docs
Makefile                     # STD_FILES += std/collections.vo
docs/reference.md, docs/errors.md (E0313), docs/roadmap.md, README.md
tests/cases/collections/, tests/cases/ordered/, tests/cases/generic/ (inference)
tests/structures_props.py, tests/structures_bench.py, tests/structures_eval.py, tests/structures_eval/
```

**Structure Decision**: one new runtime file (`src/rt_coll.c`) and one new std module; every other change
extends an existing switch at the sites the type-kind inventory lists (closed-set defaults in
`ty_name_into`, `ty_unify`, `values_equal`, `render_value`, `prop_gen` must each handle the new kinds
explicitly; a golden case per site catches a miss).

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| Four new type kinds in the checker and runtime | Equality, rendering, generation and hidden internals by contents (FR-008, FR-009), and the speed targets | The structures in vo would need opaque records plus std-only hooks for `==`, rendering and generation (more machinery), and bytecode makes SC-002 tight (user's decision in planning) |
| One loosely typed std-only primitive `__coll(op, ...)` instead of ~25 typed primitives | The library functions' vo signatures are the public types; the primitive is reachable only from `std/collections.vo` and checked by its tests | 25 builtin ids with hand-written generic checking each would triple the checker change; `ponytail:` comment names the ceiling (std code must type its calls correctly) and the upgrade (typed primitives) |
