# Implementation Plan: Structural Map Keys

**Branch**: `033-map-keys` (work on `main`, as for specs 001–032) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/033-map-keys/spec.md`

## Summary

A map key may be any type made of `int`, `bool`, `str`, `bytes`, records and variants (R1). The checker
decides it with one function that names the first part that is not a key, shaped like the JSON check of
spec 029 (R2); E0309 reports it at the type and at a call, with a `: key` fix for a bare type parameter
(R3). The map runtime keeps its `int` and `str` paths unchanged and sends every other key through one
structural hash and the existing structural `==` (R4, R5). Rendering, tasks, properties and test reports
already recurse with the key type: cases, no code (R8).

| Part | File | What |
|---|---|---|
| Rule | `src/check_generic.c`, `src/check.c`, `src/vo.h` | `key_why`, E0309 at the type, `check_targ` for `: key` |
| Syntax | `src/parse.c`, `src/fmt.c`, `src/doc.c` | the `key` constraint (`TPC_KEY`) |
| Runtime | `src/rt_map.c` | `key_hash`; structural equality for keys that are not `int`, `bool` or `str` |
| Reports, tasks, properties | `src/rt_render.c`, `src/task.c`, `src/prop.c`, `src/rt_test.c` | checked by cases, not rewritten |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/lists-and-maps.md`, `docs/roadmap.md`, `README.md` | R11 |
| Tests | `tests/cases/map_keys/`, `tests/cases/check/map_key*`, `tests/cases/bytes/type_errors`, `tests/map_keys_bench.py` | R12 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for the library and examples.

**Primary Dependencies**: none.

**Storage**: none; recordings and the effects record unchanged.

**Testing**: golden cases written first; `make quick` between edits, `make test` once per change; the
gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI) with its standard library.

**Performance Goals**: `int` and `str` keys within 5% (SC-002, same code path as today); a two-field
record key at least as fast as a `str` built from the two fields (SC-003).

**Constraints**: every valid program byte-identical (FR-004); one constraint per type parameter; maps
keep insertion order; reference ≤ 15,000 tokens (+≤ 150 here); schema 1.

**Scale/Scope**: about 150 lines of C, one catalog entry rewritten, one benchmark script.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One rule for what a key is, the same as `==`; one way to declare a key parameter (`K: key`, written like `ordered`); no tuple, so a compound key is a record, the one way to group values. | Pass |
| II. Machine-readable feedback | E0309 names the part that is not a key with its path (`Order.items` is a list), at the type and at the call; a bare parameter gets the `: key` fix. | Pass |
| III. Determinism | The hash is FNV-1a over a fixed byte order on every platform; iteration stays insertion order, which the hash never affects. | Pass |
| IV. Test-first | Golden cases per scenario before the code; existing corpus unchanged except the two E0309 cases whose programs become valid (R12). | Pass |
| V. Single source of truth, minimal surface | One constraint word, no new code, no new library function; the reference row and constraint paragraph change, +≤ 150 tokens. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/033-map-keys/
├── plan.md
├── research.md          # R1–R12
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # key types, K: key, operations
│   └── diagnostics.md   # E0309 at the type and at a call
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/check_generic.c, src/check.c, src/parse.c, src/fmt.c, src/doc.c, src/rt_map.c, src/vo.h
docs/reference.md, docs/errors.md, site/docs/guide/lists-and-maps.md, docs/roadmap.md, README.md
tests/cases/map_keys/, tests/cases/check/, tests/cases/bytes/, tests/map_keys_bench.py
```

**Structure Decision**: no new C file; the key rule sits beside the `ordered` and `json` rules in
`check_generic.c`, the hash beside the map in `rt_map.c`.

## Order of work

1. Benchmark baseline: build today's binary and keep it as the comparison (R10).
2. The rule and E0309 (R1–R3) with the `key` constraint through parse, fmt and doc.
3. The runtime hash and equality (R4–R6).
4. Reports, tasks, properties, JSON (R8): cases first, code only where a consumer bypasses the key type.
5. Reference, catalog, guide, roadmap, README; `make test` on three platforms; benchmarks; results.

## Complexity Tracking

No constitution violations.
