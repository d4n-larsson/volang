# Implementation Plan: Typed Holes, Property Tests and Contracts

**Branch**: `005-holes-props-contracts` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/005-holes-props-contracts/spec.md`

## Summary

Three language features, each small in the checker and riding on the spec 004 compiler and VM.

**1. Typed holes** (research R1–R4):
- **What it is:** `todo(desc)` is a builtin whose type is the expected type of its context, using the same expected-type path that `ok`/`err` and generic inference use.
- **Check time:**
  - Each hole gets warning W0601 with the expected type in the existing `expected` field and a new `scope` field: names in scope, with types.
  - A hole whose type cannot be determined is E0601.
  - A hole statement sets `NF_TERMINATES`.
  - A function containing a hole skips W0201 and W0501.
- **Run time:** the hole compiles to a builtin call that raises R0011.

**2. Property tests** (research R5–R10):
- **The item:** `prop "name" (params) uses ... { body }` is an `N_TEST` node with flag `NF_PROP`, its parameters kept in the FX record. It is compiled like a function with parameters.
- **The runner:** a new `src/prop.s` generates arguments from the parameters' semantic types (a separate PCG stream seeded from `--seed` and the case number), runs the body once per case under the test runner's recovery point, and shrinks a failing input greedily with type-directed candidates.
- **`assume(cond)`:** a builtin that discards the case.
- **Reports:** the existing `test` record and text lines, with added fields (`cases`, `shrinks`, `counterexample`).
- **Check time:** a parameter type that cannot be generated is E0602.

**3. Contracts** (research R11–R15):
- **Syntax:** `requires`/`ensures` clauses after `uses` on named functions, stored in the FX record.
- **Check time:**
  - Conditions are typed `bool` in the parameters' scope. `ensures` also sees `ret`, the returned value.
  - They must be pure (E0603).
- **The compiler:**
  - `requires` becomes a guarded jump at function entry that raises R0012 at the caller's call node.
  - With `ensures`, every exit (`return`, `try` propagation, falling off the end) jumps to one epilogue that checks the conditions (R0013) and then returns.
  - Functions without contracts compile exactly as today.

**4. Evaluation** (research R16):
- Each hidden-property task h01–h10 gets a property file and at least 5 mutants of the reference solution, each passing the visible cases and failing a hidden one.
- `tests/prop_eval.py` reports what the properties catch.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as. This is the existing v4 toolchain, extended in place.

**Primary Dependencies**:
- None at run time.
- Build: `binutils-riscv64-linux-gnu`.
- Running on x86: `qemu-user`.
- Lint: the SESI `riscv64` pack.
- Developer scripts: Python 3.

**Storage**: N/A. Programs, golden cases and the evaluation's mutants are files in the repository.

**Testing**:
- `make test`: golden cases, repeats, fuzzers, reference examples, catalog coverage, migrations, `gc/baseline.py`, `diff_engines.py`.
- `make gc-check`.
- `make speed-check`.
- New: `make baseline-004` and `tests/speed.py --baseline build-004/vo`, the FR-024/SC-006 check against the spec 004 binary.
- New: `tests/prop_eval.py`, the SC-001 evaluation.

**Target Platform**: Linux user space, riscv64, a static ELF with no libc. Developed under `qemu-riscv64`.

**Project Type**: Compiler and runtime for a programming language, as a CLI.

**Performance Goals**:
- Programs without contracts are unchanged: each speed-benchmark program stays within 5% of the spec 004 binary (`71375e5`), timed with alternated runs.
- A property of 100 cases, including shrinking, finishes in under 2 s for the SC-004 programs.

**Constraints**:
- Determinism (Principle III): generation and shrinking are deterministic for a given seed, version and file.
- The reference grows by at most 2,000 tokens (SC-007; it is about 8,190 now).
- Existing output stays byte-identical (FR-027).

**Scale/Scope**:
- About 2,000–2,500 new lines of assembly: about 1,200 in `src/prop.s`, the rest across the parser, checker, compiler, test runner, diagnostics and formatter.
- About 60 golden cases.
- 10 property files and at least 50 mutants.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** Each construct has one form:<br>- `todo(desc)` is an ordinary call.<br>- `prop` is an item keyword, identifiable from its first token.<br>- `requires`/`ensures` are clauses in fixed positions after `uses`.<br><br>There is one name for the returned value (`ret`). Contracts add no hidden control flow: a violation is a runtime error, like overflow. Holes are explicit in source. |
| II. Machine-readable feedback | **PASS.**<br>- New codes: W0601, E0601–E0604, R0011–R0013 (contracts/codes.md). The catalog only grows.<br>- New JSON fields are added to existing records: `scope` on `diagnostic`; `holes` on `summary`; `prop`, `cases`, `shrinks` and `counterexample` on `test`.<br>- No field is removed or renamed, so it stays schema 1.<br>- W0601 has no fix, since what fills a hole is unknown.<br>- E0601 has a fix only when exactly one annotation is unambiguous, which it never is, so it has no fix. |
| III. Determinism | **PASS.** Generation uses its own PCG stream, seeded from `--seed` (default 1), the property's position in the file, and the case number. Shrinking is a fixed greedy order with a count budget.<br><br>One reservation is recorded: when the `--timeout` deadline cuts shrinking short, the reported counterexample depends on machine speed. It is still a failing input, and the report says shrinking was cut short. That is the same class of behaviour as a timeout itself (research R9). |
| IV. Test-first through the CLI | **PASS.**<br>- Every story starts with golden cases that fail on the 004 binary.<br>- Each new code gets a case.<br>- Reference examples for holes, properties and contracts run in `ref_examples.py`, which gains a `vo-test` example with a property. |
| V. Single source of truth, minimal surface | **PASS.**<br>- `docs/reference.md` and `docs/errors.md` are updated in the same change.<br>- Growth is stated and capped at 2,000 tokens (SC-007).<br>- No CLI option is added: `--seed`, `--filter` and `--timeout` are reused.<br>- Three `ponytail:` markers: the fixed 100 cases / 1,000 attempts, the 2,000-run shrink budget, and greedy shrinking. |
| Implementation constraints | **PASS.** All runtime code is assembly with prototype comments and passes SESI lint. Python appears only in `tests/prop_eval.py`. |

No violations, so the Complexity Tracking table is empty.

**Post-design re-check:** still passes. The design adds no user-facing option, and every new report field is additive. The decision to compile contracts into the callee (R12) is what makes FR-023 (calls through function values and builtins) hold without touching the call paths.

## Project Structure

### Documentation (this feature)

```text
specs/005-holes-props-contracts/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R1–R16
├── data-model.md        # Phase 1: nodes, FX fields, runtime records, report fields
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── language.md      # grammar and semantics of todo, prop, assume, requires/ensures
│   ├── reports.md       # diagnostic, test and summary output (text and JSON)
│   └── codes.md         # new catalog entries
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── defs.inc          # T_PROP/T_REQUIRES/T_ENSURES, NF_PROP, FX_* fields, D_SCOPE, builtin ids
├── lex.s             # three keywords
├── parse.s           # prop item, requires/ensures clauses
├── check.s           # prop parameters (E0602), contract scopes, `ret`, hole bookkeeping
├── check_expr.s      # todo (expected type, W0601 + scope, E0601), assume (E0604)
├── check_func.s      # NF_TERMINATES for a hole statement; W0201 suppression
├── check_effects.s   # contract purity (E0603); W0501 suppression when a function has a hole
├── compile.s         # todo, assume, requires prologue, ensures epilogue
├── vm.s              # (expected: no new opcode; contract failures are builtin calls)
├── rt_call.s         # rt_hole (R0011), rt_assume, rt_requires_fail (R0012), rt_ensures_fail (R0013)
├── prop.s            # NEW: generator, shrinker, property runner
├── testrun.s         # dispatches NF_PROP to prop.s; cases/shrinks/counterexample in reports
├── diag.s            # D_SCOPE in JSON and text; `holes` in the check summary
├── fmt.s             # prop items, contract clauses
├── effects_record.s  # `prop "name"` entries
└── main.s            # (link prop.s; no new option)

docs/reference.md, docs/errors.md

tests/
├── cases/hole/, cases/prop/, cases/contract/   # golden cases (new directories)
├── prop_eval/hNN-*/props.vo, prop_eval/hNN-*/bugs/*.vo
├── prop_eval.py                                 # SC-001/SC-002 evaluation
└── (run.sh, catalog.py, ref_examples.py pick the new cases up unchanged)
```

**Structure Decision**: the existing single-project layout, with one new source file (`src/prop.s`), because generation and shrinking are a self-contained runtime component. Everything else extends the file that already owns that stage.

## Complexity Tracking

No constitution violations.
