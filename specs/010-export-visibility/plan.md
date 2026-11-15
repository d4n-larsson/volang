# Implementation Plan: Visibility with `export`

**Branch**: `010-export-visibility` | **Date**: 2026-11-15 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/010-export-visibility/spec.md`

## Summary

`export` before a top-level `fn`, `record` or `variant` makes it importable; every other item is private
to its file (research R1). Importing a private item is **E0209** `not_exported`, with a fix inserting
`export ` before the declaration when it is in a program file, and no fix for library and package items
(R2). An exported item whose signature, field or case names a private type of its file is **E0210**
`private_type_exported`, with the same fix on the type (R3). One diagnostic per private declaration
carries the fix; further import sites are related notes (R4). The standard library exports exactly its
documented items (R5); `vo doc` lists exported items (R6). The repository's multi-file programs migrate by
the same edit the fix makes (R7). The IntelliJ plugin learns the keyword (R8).

## Technical Context

**Language/Version**: C (gnu11, freestanding; spec 008) for the toolchain; vo for `std/`; Kotlin for the
IntelliJ plugin (`../volang-intellij-plugin`).

**Primary Dependencies**: none at run time; GCC for x86-64, ARM64 and RISC-V; qemu-user; Python 3 for
tests and the one-off migration; Gradle for the plugin.

**Storage**: N/A.

**Testing**: golden cases (`tests/cases/{multi,parse,check,doc,fmt,query,edit}`), `tests/catalog.py`,
`tests/apply_fixes.py`, `tests/ref_examples.py`, `tests/doc_conformance.py` (exported library items are
exactly the documented ones), every multi-file suite (`effect_changes`, `refactor_eval`, `stdlib_eval`,
`prop_eval`, `packages_check`), the full `make gate`; plugin lexer tests.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu); IntelliJ 2026.2.

**Project Type**: compiler and runtime of a programming language, as a CLI; an editor plugin.

**Performance Goals**: none: one flag test per import and one walk over exported signatures.

**Constraints**: Principle I (one form: the keyword on the declaration, nothing else); Principle II (fixes
for every unambiguous correction; fixes never collide); the reference grows by at most 250 tokens.

**Scale/Scope**: about 150 lines of C; 48 library items gain `export`; about 990 imports in the repository
(`bench2/`, golden cases, evaluation suites, examples) migrated mechanically.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** One form (`export` on the declaration), first-token rule kept (an item starts with `export`, `fn`, `record`, `variant`, `test`, `prop` or `import`); no re-export, no export lists. |
| II. Machine-readable feedback | **PASS.** E0209/E0210 carry fixes when the declaration is editable; library and package items say why there is none. One diagnostic per declaration keeps fixes from colliding. No schema change. |
| III. Determinism | **PASS.** Visibility is checked statically; run-time behavior is unchanged. |
| IV. Test-first through the CLI | **PASS.** Golden cases for both codes, misplaced `export`, std privacy, packages, `vo doc`, `vo fmt`, `vo edit rename`, written before the code. |
| V. Single source of truth, minimal surface | **PASS.** Reference, catalog and `vo doc` updated in the same change; one keyword. |
| Implementation constraints | **PASS.** C with zero warnings on all three architectures; no assembly. |

## Project Structure

### Documentation (this feature)

```text
specs/010-export-visibility/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/language.md   # syntax, rules, messages, fixes
├── contracts/codes.md      # E0209, E0210 (read by tests/catalog.py)
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
src/vo.h          # T_EXPORT 47, T_KW_LAST 47, NF_EXPORT 512
src/lex.c         # "export" in kw_table
src/parse.c       # `export` before fn/record/variant; misplaced `export`
src/loader.c      # bind_imports: E0209 (fix or not, one per declaration)
src/check.c       # E0210 over exported items' type nodes
src/fmt.c         # prints `export `
src/doc.c         # lists exported items
std/*.vo          # `export` on the documented items
docs/reference.md, docs/errors.md
tests/catalog.py, tests/doc_conformance.py, tests/packages_check.py, tests/fuzz_inputs.py, tests/speed.py
tests/cases/...   # new cases; migrated multi-file cases
bench2/, tests/{effect_changes,refactor_eval,stdlib_eval,prop_eval,...}, examples/  # migrated
../volang-intellij-plugin/src/main/kotlin/volang/Vo.kt  # keyword
```

**Structure Decision**: no new source files; each rule lives where its mechanism already is.

## Complexity Tracking

No violations to justify.
