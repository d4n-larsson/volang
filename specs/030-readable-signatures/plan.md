# Implementation Plan: Readable Signatures and Named Effect Sets

**Branch**: `030-readable-signatures` (work on `main`, as for specs 001–029) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/030-readable-signatures/spec.md`

## Summary

Two changes that keep `uses` as the one form for effects. (1) `vo fmt` lays out item headers over 100
characters: the effect clause on its own line, then, if still needed, the parameters one per line (research
R7). (2) Named effect sets: `effects Storage = file.read("data/"), file.write("data/");`, usable wherever an
effect is written (R1, R3), expanded when a clause is turned into a set so that records, diagnostics, `vo
doc` and `vo query` stay exact without other changes (R2, R8); fixes add effects next to a set name, and
W0501 does not nag about a set's unused parts (R5).

| Part | File | What |
|---|---|---|
| Parsing | `src/parse.c`, `src/vo.h` | the `effects` item (`N_EFFECTS`), `export effects` |
| Checking | `src/check.c`, `src/check_effects.c` | set symbols, expansion with cycle check (E0513), names in clauses and types, E0509 for sets, W0505, fixes beside set names, W0501 rule |
| Formatting | `src/fmt.c` | the `effects` item; clause order (effects, then set names); header layout |
| Docs and tools | `src/doc.c`, `src/query.c`, `src/edit.c` | `vo doc` lists sets; references and renames include sets |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/effects.md` | R9 |
| Examples | `examples/notes-api/`, `bench/web/reference/` | a `Storage` set; all formatted |
| Tests | `tests/cases/effect_sets/`, `tests/cases/fmt/`, `tests/effect_sets_records.py` | R10 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for examples.

**Primary Dependencies**: none.

**Storage**: none (the effects record's format is unchanged).

**Testing**: golden cases, `fmt_corpus.py`, `migrate.py`, `doc_conformance.py`, `site_examples.py`, a new
`tests/effect_sets_records.py`; `make quick` between edits, `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` time unchanged within noise (sets are expanded once each).

**Constraints**: one form for effects; reference ≤ 15,000 tokens (+≤ 150 here); existing programs and
records unchanged (SC-005); formatting idempotent.

**Scale/Scope**: about 400 lines of C (parse 60, checker 200, fmt 120, doc/query/edit 20), docs, two
examples.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form: sets are written where effects are written, with the same clause syntax; no annotation spelling (rejected in the spec). The layout is the formatter's, not a choice. `effects` is not a new reserved word. | Pass |
| II. Machine-readable feedback | E0509 extended, E0513 and W0505 new, each with span, message and fixes (nearest name, remove the declaration); every report shows expanded effects. | Pass |
| III. Determinism | Expansion is static; formatting deterministic and idempotent. | Pass |
| IV. Test-first | Golden cases per scenario and diagnostic before the code; fmt cases for each layout state. | Pass |
| V. Single source of truth, minimal surface | Reference, catalog, contracts together; +≤ 150 tokens; no parameters on sets, no set algebra beyond union (YAGNI). | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/030-readable-signatures/
├── plan.md
├── research.md          # R1–R10
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # the effects item, meaning, canonical order
│   ├── diagnostics.md   # E0509, E0513, W0505, fixes, W0501
│   └── format.md        # header layout
└── tasks.md
```

### Source Code (repository root)

```text
src/parse.c, src/check.c, src/check_effects.c, src/fmt.c, src/doc.c, src/query.c, src/edit.c, src/vo.h
docs/reference.md, docs/errors.md, site/docs/guide/effects.md
examples/notes-api/, bench/web/reference/
tests/cases/effect_sets/, tests/cases/fmt/, tests/effect_sets_records.py
```

**Structure Decision**: no new source file: sets live where effects are parsed and resolved; the layout
lives in the formatter beside the statement rule.

## Order of work

1. Header layout in `vo fmt` (independent of sets; examples reformatted).
2. The `effects` item: parse, symbols, expansion, cycle check, use in clauses and types.
3. Diagnostics: E0509 for sets, E0513, W0505; fixes beside set names; the W0501 rule.
4. `vo fmt` for sets and clause order; `vo doc`, `vo query`, `vo edit rename`.
5. Examples, docs, the records check, `make test` on three platforms; SC-004 run with the user.

## Complexity Tracking

No constitution violations.
