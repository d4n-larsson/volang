# Implementation Plan: vo-lang v2 Core: Effects, Functions as Values, Generics

**Branch**: `002-effects-functions-generics` | **Date**: 2026-11-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-effects-functions-generics/spec.md`

## Summary

vo v2 extends the v1 toolchain (RV64 assembly, `specs/001-vo-lang-core`) with three type-system features designed together:

1. **Checked effects with scopes.** Effects are declared after the return type with `uses …`.
   - The checker demands that every function covers the effects of everything it calls.
   - A run's grant is exactly what `main` declares; a test's grant is what the test declares.
   - The runtime enforces file and environment scopes against the innermost user function's declaration.
   - A plain-text `vo.effects` record turns capability changes into a reviewable diff that `vo check` gates.
2. **Functions as values.** Anonymous `fn(…) { … }` expressions use the same syntax as named functions. They capture `let` values and parameters by value, and their effects are inferred from the body. Function types are `fn(T) -> R uses E`, and named functions are values. New builtins: `sort_by`, `map`, `filter`, `fold`, `any`, `all`.
3. **Generics.**
   - Functions, records and variants take type parameters (`[T]`, `[T: ordered]`), and effect parameters (`[E: effects]`) for higher-order functions.
   - Type arguments are only ever inferred, from arguments or the expected type, never written at call sites. That keeps one spelling and avoids the `f[x](…)` ambiguity.
   - Generic bodies are checked once.
   - At run time, values stay untagged. Generic frames and generic record/variant types carry type arguments, so equality, rendering and sorting resolve type parameters.

Alongside the features: one-round migration of v1 programs through fixes, and a harder **benchmark v2** (`bench2/`, 40+ tasks in 4 categories) that runs vo and Python the same way, with capability violations observed in both.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as. This is the existing v1 toolchain, extended in place.

**Primary Dependencies**: None at runtime. To build: `binutils-riscv64-linux-gnu`. To run on x86: `qemu-user`. To lint: the SESI `riscv64` pack.

**Storage**: Source files, plus a `vo.effects` record per project root (plain text).

**Testing**: The v1 harness, extended: golden cases (new areas `effects/`, `func/`, `generic/`), `apply_fixes.py`, `catalog.py`, `ref_examples.py`, `fmt_corpus.py`, the fuzzer (with new syntax mutations) and a scope fuzzer (`tests/scope_fuzz.py`). Benchmark v2 lives in `bench2/`.

**Target Platform**: Linux user space, RV64 (qemu-riscv64 on x86 hosts).

**Project Type**: Compiler front end and interpreter, as a CLI.

**Performance Goals**: Check plus run of a 1,000-line v2 program in under 1 s under qemu (SC-008). v1 measured 63 ms.

**Constraints**:
- Reference of 15,000 tokens or less, including v2 (v1 is at ~6.5k now).
- Deterministic output, except clock and randomness, which `--clock-ms` and `--seed` can fix.
- Every new construct has one spelling and a first-token statement.

**Scale/Scope**: About 6–8k additional lines of assembly, 17 new diagnostic codes, 1 new runtime code, 8 new builtins, and benchmark v2 with 40+ tasks plus a 20-task feature set.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluated against constitution **v1.1.0**.

| Gate | Status | Evidence / obligations |
|---|---|---|
| I. Agent-first design | PASS | Anonymous functions reuse the `fn` syntax. Type arguments are never written at call sites (inference plus annotations). `uses` clauses have one canonical order (contracts/grammar-v2.md). Effects are explicit on named functions. |
| II. Machine-readable feedback | PASS | New codes with fixes (contracts/error-codes-v2.md). New JSONL records `effect_change` and `capabilities`. Schema version 1 is extended with new fields and kinds only (contracts/output-schema-v2.md). |
| III. Determinism and defined behavior | PASS | Clock and randomness are effects. `--clock-ms` and `--seed` make them reproducible, and `vo test` fixes both by default. Scope violations are R0010 with a structured report. |
| IV. Test-first through the CLI | PASS | **Obligation**: golden cases precede each implementation task; every new code gets a golden case; the migration corpus runs as a test (`tests/migrate.py`). |
| V. Single source of truth, minimal surface | PASS | The reference is updated in the same tasks. The budget is checked by `ref_examples.py` (estimate: +3.5k tokens, ~10k total). **Obligation**: new shortcuts get `ponytail:` markers (e.g. heuristic rename detection). |
| Implementation constraints | PASS | Assembly only. New files are lint-clean. Python is used only in `tests/` and `bench2/`. |
| Quality gates | PASS | `make test` gains `migrate.py`, `record_changes.py` and `scope_fuzz.py`, and the determinism check covers the new run areas. |

**Post-design re-check**: PASS. No new dependencies. The design adds no requirements beyond the spec.

## Project Structure

### Documentation (this feature)

```text
specs/002-effects-functions-generics/
├── plan.md
├── research.md            # design decisions (Phase 0)
├── data-model.md          # types, effects, closures, records, runtime layouts
├── quickstart.md          # validation scenarios
├── contracts/
│   ├── grammar-v2.md      # syntax and semantics added to contracts/grammar.md of 001
│   ├── cli-v2.md          # new command and flags
│   ├── output-schema-v2.md# new records and fields
│   ├── error-codes-v2.md  # new codes
│   ├── effects-record.md  # vo.effects format and comparison rules
│   └── benchmark-v2.md    # task categories, grading, harness protocol
└── tasks.md               # /speckit-tasks
```

### Source Code (repository root)

```text
src/
├── defs.inc        # + effect bits, TY_FUNC/TY_PARAM/TY_EFFVAR, node kinds for lambdas/fn types/type params
├── parse.s         # uses clauses, fn types, lambdas, [T]/[T: ordered]/[E: effects], call on any expression
├── types.s         # NEW: generic types: instantiation, substitution, unification for inference
├── effects.s       # NEW: effect sets with scopes: union, coverage, render; vo.effects read/compare/update
├── check.s         # + effect checking per function/test, capture rules, generic declarations
├── check_expr.s    # + lambdas, call values, generic inference, function-type compatibility, new builtins
├── interp.s        # + closures, calls through values, type-argument frames, scope checks
├── rt_call.s       # + sort_by/map/filter/fold/any/all/env/random_int
├── rt_io.s         # + scope enforcement for file operations and env reads; seed/clock control
├── fmt.s           # + canonical printing of the new syntax
└── main.s          # + `vo effects`, --seed, --clock-ms
docs/reference.md   # v2 sections (effects, functions as values, generics)
docs/errors.md      # new codes
tests/cases/{effects,func,generic}/   # new golden areas
tests/migrate/v1/   # frozen v1 corpus with recorded outputs (SC-005)
tests/effect_changes/ + tests/record_changes.py   # SC-004 change sets
tests/migrate.py    # SC-005: v1 corpus migrates in one round, identical output
tests/scope_fuzz.py # SC-003a: randomized paths and env names against scopes
bench2/             # benchmark v2: tasks/, harness (multi-file replies, hidden checks, capability grading)
```

**Structure Decision**: Extend the single v1 project. Two new source files keep the new concepts out of the already-large checker files: `types.s` (generic types) and `effects.s` (effect sets and the record). The benchmark gets its own directory because its protocol differs (multi-file replies, hidden checks, sandbox observation for Python).

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each with its ceiling:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Type erasure plus runtime type-argument frames (no monomorphization) | Generic equality and rendering pay a substitution walk | Specialize hot generic functions per instantiation |
| Rename detection in `vo.effects` by "removed and added with identical effects in the same file" | Two simultaneous renames with identical effects can pair up wrongly; reported as renames, which reviewers can still read | Match by body similarity |
| Python capability observation through audit hooks plus a patched `os.environ` | Misses effects done through C extensions or `ctypes` | Run under seccomp or strace |
| Captures by value only, no `var` capture | Some idioms need an explicit copy | None planned (it's the design) |
