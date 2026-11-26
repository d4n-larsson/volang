# Implementation Plan: Anonymous Functions Take Their Types From Context

**Branch**: `059-inferred-function-types` (work on `main`, as for specs 001–058) | **Date**: 2026-11-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/059-inferred-function-types/spec.md`

## Summary

An anonymous function writes parameter names only (`fn(p) => p.age`, `fn(acc, x) { ... }`); the checker
passes the expected function type into `check_lambda`, which today drops it (research R2). Generic calls
check every non-lambda argument first, then lambdas left to right, inferring an open return type from the
body (R3, R4). Written types are E0342 with a one-edit fix that removes them, or moves them onto the
`let` (R5); a single-`return` block body is E0413, rewritten to `=> expr` by its fix and by `vo fmt` (R6);
a lambda with no expected type is E0343 (R7). Compile, traces, runtime frames and hover already read
resolved types, so they show the inferred ones unchanged. Ships as 0.5.0 with the repository migrated by
its own fixes (R9, R11).

| Part | File | What |
|---|---|---|
| Lexing and parsing | `src/lex.c`, `src/parse.c`, `src/vo.h` | `T_FAT_ARROW`; names-only parameters; `=> expr` body (stored as a one-`return` block flagged `NF_ARROW`); written types still parsed (R1) |
| Expected types | `src/check_func.c`, `src/check_expr.c`, `src/check.c` | `check_lambda(node, expected, open)`; list elements; `map_err`; return collection (R2, R4) |
| Generic inference | `src/check_func.c` (`check_hof`), `src/check_generic.c` | non-lambda arguments first, lambdas left to right; E0343 for an open parameter type (R3, R7) |
| Diagnostics and fixes | `src/check_func.c`, `src/diag.c` | E0342 (remove or move to `let`), E0343, E0413 (R5–R7) |
| Formatting | `src/fmt.c` | names-only parameters, `=> expr`, the single-return rewrite (R6, R8) |
| Editors | `editors/`, `site/pygments_vo` | `=>` in three grammars and the site lexer (R8) |
| Migration of the repo | `std/`, `packages/postgres/`, `examples/`, `tools/vo_fix.vo`, `site/docs/`, `docs/`, `README.md` | by fixes; std sites without an expected type become named functions (R9) |
| Docs | `docs/reference.md`, `docs/errors.md`, `docs/stability.md`, `CHANGELOG.md`, `site/docs/guide/functions.md` | R12 |
| Tests | `tests/cases/lambda_types/` (new), `tests/migrate_059.py` (new), `tests/apply_fixes.py`, `tests/fmt_corpus.py` | R11 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for std, packages, examples and tools.

**Primary Dependencies**: none.

**Storage**: none. Recordings, `vo.effects` and `vo.packages` are unchanged; JSON schema 1 is unchanged
(new codes only).

**Testing**: golden cases written first and failing (`tests/cases/lambda_types/`); `apply_fixes.py`,
`catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py`, `fmt_corpus.py`, `fuzz.py`,
`editors/check.js`, `lsp.py`, new `migrate_059.py`; `make quick AREA=front,docs` between edits, `make test`
once per change, `make gate` (now nightly in CI) before results.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` of the largest example and of the generated 8,000-item program within 5%
of today (SC-006). No run-time change: the bytecode of a migrated program is identical.

**Constraints**: one form (written types are an error with a fix, not optional; `=> expr` is the only
single-return form); local inference only; one edit per fix (schema 1); reference ≤ 15,000 tokens with
**no margin today** (R12): the change must not grow it.

**Scale/Scope**: about 650 lines of C (lexer and parser 60, `check_lambda` and expected-type plumbing 200,
generic order 120, the three diagnostics and fixes 150, fmt 70, tools 0: they read resolved types); 3 new
codes; 280 lambdas in golden cases, 128 in std and examples, 74 in docs migrated by fixes.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form: an anonymous function never writes types (E0342 with a fix, not an option), and a single-return body has one form, `=> expr` (E0413; `vo fmt` rewrites it). Every statement still starts with a keyword or is a call; `=>` occurs only after an anonymous function's parameters. No implicit conversions or shadowing. **Tension**: "explicit verbosity is preferred". The types are not hidden: the expected type is written at the position (the parameter of `map`, the field, the binding), hover, `type-at`, traces and runtime frames show them (FR-007), and a wrong body is E0301 at the expression naming the expected type. Decided with the user on 2026-11-25. | Pass (justified below) |
| II. Machine-readable feedback | Three new codes with catalog entries; E0342 and E0413 have one-edit fixes whose results check clean; E0343 has none because the types are unknown. FR-005: a written type that differs reports today's mismatch first, so applying fixes never changes meaning. No cascades: a lambda with E0343 is checked with error-typed parameters. | Pass |
| III. Determinism | No run-time change; migrated programs compile to the same bytecode and give byte-identical output on three platforms (SC-002, `migrate_059.py`). | Pass |
| IV. Test-first | Golden cases per scenario and per code before the code; every reference and guide example executed; `migrate_059.py` proves the migrated cases keep their meaning; catalog entries with broken and fixed examples. | Pass |
| V. Single source, minimal surface | Reference, catalog and contracts change together; the reference must not grow (R12) and its size before and after is stated. Left out (YAGNI): inference across statements, typed parameters as an option, `=>` on named functions. | Pass |
| VI. Backend network services | **Write**: data handling in a service (sorting, mapping, folding rows; route and step tables) is where lambdas are, and an agent stops restating types the call already fixes; single-return lambdas drop from three lines to one. **Review**: a reviewer reads `sort_by(orders, fn(o) => o.due_ms)` at a glance; the types stay one hover away and in traces. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/059-inferred-function-types/
├── plan.md
├── research.md          # R1–R13
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # grammar, inference order, canonical form
│   └── diagnostics.md   # E0342, E0343, E0413, fixes
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/lex.c, src/parse.c, src/vo.h, src/check.c, src/check_expr.c, src/check_func.c, src/check_generic.c,
src/fmt.c
editors/vscode, editors/tree-sitter-vo, editors/vim, site/pygments_vo
std/*.vo, packages/postgres/*.vo, examples/*, tools/vo_fix.vo
docs/reference.md, docs/errors.md, docs/stability.md, CHANGELOG.md, README.md, site/docs/
tests/cases/lambda_types/, tests/migrate_059.py
```

**Structure Decision**: no new source file. The inference lives in `check_lambda` and `check_hof`
(`check_func.c`) and `check_generic_call` (`check_generic.c`), next to the unification it reuses.

## Order of work

Each step ends with its golden cases passing and `make quick`; the feature is usable after step 3.

1. **Syntax (R1)**: `=>`, names-only parameters, `NF_ARROW`; parse-only golden cases; fmt prints both
   forms (R8).
2. **Expected types (R2, R4)**: `check_lambda(node, expected, open)`, return collection, list elements,
   `map_err`; US1 scenarios 1, 3, 4 for non-generic positions.
3. **Generic order (R3, R7)**: `check_hof`, `check_generic_call`, generic payloads; E0343; US1 complete.
4. **Migration diagnostics (R5, R6)**: E0342 with both fixes, E0413, fmt's rewrite; FR-005 ordering; US2
   and US3 cases; `vo_fix` rounds.
5. **Repository (R9, R11)**: migrate `std/` (named functions where needed), `packages/postgres`,
   `tools/vo_fix.vo`, examples, then golden cases with `migrate_059.py`; `make test`.
6. **Editors (R8)**: grammars and `editors/check.js`.
7. **Docs (R12)**: reference (net ≤ 0 tokens), catalog, guide, stability, changelog, README; budget check.
8. `make test` on three platforms, `make gate`, check-time measurements (SC-006), header and line counts
   (SC-001, SC-003), `results.md`; migrate the voledger and vositegen kits with the user.

## Complexity Tracking

| Choice | Simpler alternative | Why not |
|---|---|---|
| Two body forms chosen by shape (`=>` for a single `return`, a block otherwise) | block bodies only | US3, decided with the user: single-return lambdas are most lambdas (54 of 83 lambda lines in `examples/`); the shape decides the form, so there is still one way to write each body |
| Non-lambda arguments first, then lambdas (R3) | source order | `serve_workers(address, workers, setup, fn(ctx, r) ...)` and `fold(xs, 0, fn(acc, x) ...)` need their type variables fixed before the lambda's parameters; source order fails the first when `setup` follows |
| Written types parsed and reported by the checker (R1, R5) | a parse error | `vo fmt` refuses files with parse errors, so it could not format the 0.4 code it migrates |
