# Implementation Plan: Spec Annotations on Tests

**Branch**: `012-spec-annotations` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/012-spec-annotations/spec.md`

## Summary

`@spec("FR-002", "US1/AC1")` on the line above a `test` or `prop` (research R1). The lexer gains `@`; the
parser reads annotations in front of any top-level item and keeps them on the item; the checker validates
them with four errors and a warning, all with fixes where one correction is clear (R2). `vo test` reports
each test's IDs and, for annotated programs, a per-ID result summary (R3); `vo query specs ENTRY` lists
every ID with its tests, statically (R4). `vo fmt` prints annotations above their item with IDs sorted
(R5); the IntelliJ plugin highlights them (R6).

## Technical Context

**Language/Version**: C (gnu11, freestanding) for the toolchain; Kotlin for the plugin.

**Primary Dependencies**: none at run time; GCC for three architectures; Python 3 for tests; Gradle.

**Storage**: N/A.

**Testing**: golden cases (`tests/cases/spec/`, `fmt`, `query`), `tests/catalog.py`, `tests/apply_fixes.py`,
`tests/ref_examples.py`, an agent check (SC-004), the full gate; plugin lexer tests.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu); IntelliJ 2026.2.

**Project Type**: compiler and runtime of a programming language, as a CLI; an editor plugin.

**Performance Goals**: none.

**Constraints**: Principle I (the first-token rule: an item may start with `@`; one spelling per
annotation); Principle II (fixes; JSON fields added, none changed); the reference grows by at most 200
tokens; unannotated programs byte-identical.

**Scale/Scope**: about 300 lines of C, 5 catalog codes, 15 golden cases, a plugin token.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** One form (`@spec("ID", ...)` on its own line above the item); a closed set, so an agent cannot invent annotations; items still start with a fixed token (`@` or a keyword). |
| II. Machine-readable feedback | **PASS.** Every mistake has a code and, where one correction is clear, a fix (closest name, merge, drop a repeat); `vo test --json` and the query add records/fields within schema 1. |
| III. Determinism | **PASS.** Annotations change no behavior; reports are sorted. |
| IV. Test-first through the CLI | **PASS.** Cases first; each new code gets a catalog entry and a case. |
| V. Single source of truth, minimal surface | **PASS.** One annotation; the reference, catalog and `contracts/` change together; the per-ID summary reuses `vo test` instead of a second runner. |
| Implementation constraints | **PASS.** C only, zero warnings. |

## Project Structure

### Documentation (this feature)

```text
specs/012-spec-annotations/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/language.md   # syntax, rules, diagnostics, reports, the query
├── contracts/codes.md      # E0116-E0119, W0101 (read by tests/catalog.py)
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
src/vo.h                 # T_AT; Fx gains `specs` (FX_SIZE 136)
src/lex.c, src/parse.c   # `@`, annotations in front of items
src/check.c              # validation: E0116-E0119, W0101
src/fmt.c                # printing above the item, IDs sorted
src/testrun.c            # `spec` in test records, the human failure line, the per-ID summary
src/query.c              # `vo query specs ENTRY`
docs/reference.md, docs/errors.md, tests/catalog.py
tests/cases/spec/, tests/cases/fmt/, tests/cases/query/
../volang-intellij-plugin   # ANNOTATION token
```

**Structure Decision**: no new source files; annotations live on the item's existing side record (Fx).

## Complexity Tracking

No violations to justify.
