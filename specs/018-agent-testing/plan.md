# Implementation Plan: Agent-Friendly Testing

**Branch**: `018-agent-testing` | **Date**: 2026-11-17 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/018-agent-testing/spec.md`

## Summary

`vo test` gains discovery, explained failures, coverage and mutation testing, all through its CLI and
`--json` records. There is no new syntax and no new program-visible builtin. Research R1-R14 covers
each piece:
- **Discovery and selection** (R1-R3): identifiers `FILE::NAME`. `--list` checks and lists without
  compiling. `--test ID` and `--spec ID` select exactly, and `--diff FILE` selects through `vo impact`'s
  own steps. An unknown ID is T0011 with the closest identifiers.
- **Explained failures** (R4-R7):
  - the compiler gives `expect(comparison)` both operand values (`BU_EXPECT_CMP`) and splits
    `expect(a and b)` into two checks;
  - `expect_eq` walks both values to the first difference;
  - every non-pass record gets a `cause` from a fixed list;
  - standard output is captured per test into a static 64 KiB buffer.
- **Coverage** (R8): with `--coverage`, the compiler emits `OPC_COVER k` at function, lambda, branch,
  case and loop entries. The handler sets a byte in a shared mapping, so tasks count too. The program's
  own files are reported in source order.
- **Test quality** (R9-R11):
  - two check-time warnings, W0602 (no check) and W0603 (constant check);
  - `--mutate` runs a baseline that records which tests reach which functions, then one forked child
    per type-preserving change. The child recompiles and runs only the reaching tests, with a time limit
    from the baseline;
  - survivors are reported in order and capped by `--max-mutants`. T0012 is raised when the baseline
    fails.
- **Planning amendments** to the spec: FR-005 (`or` names both parts), FR-011 (the body alone, since
  `expect` cannot be in functions), FR-019 (the new `id` field changes JSON test goldens), SC-005 (a
  fixture set instead of bench2).

## Technical Context

**Language/Version**: C (C11, freestanding, the project's strict warning set) for the toolchain; vo for
fixtures and tests; Python 3 for the new test scripts.

**Primary Dependencies**: none new. It reuses:
- the impact steps (`src/query.c`);
- `capture_trace` (`src/vm.c`);
- the property runner's final rerun (`src/prop.c`);
- fork and wait as in `src/task.c` and `src/process.c`;
- `values_equal` and `render_value`;
- the "did you mean" ranking.

**Storage**: none. Shared anonymous mappings hold coverage hits and reach bytes during a run.

**Testing**:
- golden cases in `tests/cases/test/` (listing, selection, T0011, operands, differences, every cause,
  output, W0602, W0603), `tests/cases/coverage/` and `tests/cases/mutate/`;
- step cases in `tests/cases/test_steps/` (`--diff` with a diff file, `--list` leaving files untouched);
- `tests/test_scale.py` (SC-001, SC-004) and `tests/mutate_eval.py` with `tests/mutate_eval/` fixtures
  (SC-005);
- the determinism step for the new cases, and the full gate on three platforms, instrumented.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V 64 (native or qemu-user).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**:
- listing 200 tests in under 1 s;
- coverage overhead at most 1.5×;
- `--mutate` on each `examples/` program in under 60 s at the default cap;
- the speed gate stays under 1.10, because nothing is emitted without the options.

**Constraints**:
- Principle II: four catalogued codes; schema 1 additions only.
- Principle III: listing, coverage and mutation order fixed; shared bytes set idempotently.
- Principle IV: golden cases first.
- Principle V: reference +700 tokens at most, estimated 550, now about 11,687 of 15,000.
- No new assembly.

**Scale/Scope**: about 1,100 lines of C:
- `src/testrun.c`: listing, selection, causes, capture, coverage and mutation reports (~350);
- `src/mutate.c` new: change generation, fork and wait, survivors (~300);
- `src/compile.c` and `src/vm.c`: `OPC_COVER`, the coverage table, the `expect` shapes, the
  `not`-skip flag and compiler reset (~150);
- `src/rt_test.c`: `rt_expect_cmp` and the difference walk (~200);
- the checker warnings in `src/check_expr.c` or `src/check.c` (~60);
- option parsing and the impact reuse in `src/main.c` and `src/query.c` (~60).

It also adds 4 codes, ~60 golden cases, ~6 step cases, 2 scripts and about 6 mutation fixtures.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No new syntax, keywords or builtins visible to programs; `expect` and `expect_eq` keep one form each (operands and differences come from the existing checks, R5, R6). Each new option does one thing; identifiers are made only of what reports already show (R1). |
| II. Machine-readable feedback | **PASS.** Every new report is a JSON record within schema 1 (fields and kinds added, none renamed); causes are a fixed documented list (R7); W0602, W0603, T0011 and T0012 get catalog entries, `vo explain` text and golden cases. |
| III. Determinism and defined behavior | **PASS.** Listing, selection, coverage and mutation follow fixed orders. Coverage bytes are set idempotently, so concurrent tasks cannot change them. Mutant children are bounded by a derived limit and killed; a change that only slows a test near the limit is the one known source of variation (R11), and it is documented. Capture is bounded (64 KiB). |
| IV. Test-first through the CLI | **PASS.** Every scenario, cause, operator, difference kind, coverage point kind and code lands first as a failing golden or step case; the new cases join the determinism step. |
| V. Single source of truth, minimal surface | **PASS.** Reference, catalog and contracts change together (contracts/language.md, codes.md); about 550 tokens. Recorded tests, isolation, skips, retries and mocks are left out with reasons (spec). Marked shortcuts: the 64 KiB capture, the 65,536 coverage points, reach as a byte matrix, one `+1` change per literal, sequential mutant children. |
| Implementation constraints | **PASS.** C only, no new assembly. Forked children use the same patterns as tasks; buffers have explicit limits; strict warnings and the instrumented build cover the new file; all three platforms are gated. |
| Workflow and quality gates | **PASS.** Reference growth is stated in the change; CLI and schema changes are reviewed against I, II and V above; the full gate runs on x86-64 and `make test` on ARM64 and RISC-V. |

**Post-design re-check (after Phase 1)**: unchanged, all PASS. The design amends four spec lines,
FR-005, FR-011, FR-019 and SC-005, each marked "amended in planning" in the spec.

## Project Structure

### Documentation (this feature)

```text
specs/018-agent-testing/
├── plan.md              # This file
├── research.md          # Phase 0: R1-R14
├── data-model.md        # Phase 1: entries, selection, results, coverage points, reach, mutants
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── cli.md           # options, text and JSON reports
│   ├── codes.md         # W0602, W0603, T0011, T0012; new triggers of T0001, T0003, T0007
│   └── language.md      # the reference text
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── testrun.c      # identifiers, --list (test_entry), selection (--test/--spec/--diff), causes,
│                  #   capture buffer and output reports, coverage report, test summary fields
├── mutate.c       # NEW: change generation (R10), baseline reach, fork/wait/kill per change,
│                  #   mutant and mutation_summary reports, T0012
├── rt_test.c      # rt_expect_cmp (operands), the first-difference walk for expect_eq
├── rt_io.c, task.c  # standard output into the capture buffer while a test runs
├── compile.c      # expect shapes (comparison, and, not), OPC_COVER points, the not-skip flag,
│                  #   compile reset for mutant children
├── vm.c           # OPC_COVER handler (both tables), in_test flag in capture_trace
├── prop.c         # clear the capture before the final counterexample run
├── check.c / check_expr.c  # W0602, W0603
├── query.c        # the impact steps callable from vo test (r_test entries)
├── main.c         # options and their only_with rules, cmd_test flow (list / run / coverage / mutate)
├── rt_call.c      # BU_EXPECT_CMP
└── vo.h           # OPC_COVER, BU_EXPECT_CMP, prototypes, shared globals
docs/reference.md, docs/errors.md   # contracts/language.md; four catalog entries
site/docs/guide/tests.md            # the guide page gains listing, explained failures, coverage, mutation
tests/cases/test/, tests/cases/coverage/, tests/cases/mutate/, tests/cases/test_steps/
tests/test_scale.py, tests/mutate_eval.py, tests/mutate_eval/
tests/catalog.py                    # 018's codes.md
Makefile                            # the new scripts in test/gate; new cases in the determinism step
```

**Structure Decision**: the existing single-binary layout. Test reporting stays in `src/testrun.c`.
Mutation testing gets its own file because it owns a process lifecycle (fork per change) that nothing
else in the test runner has.

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each marked `ponytail:` in code:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Static 64 KiB capture per test (R4) | longer output is cut, with a note | a growable buffer outside the per-test heap |
| 65,536 coverage points (R8) | larger programs get T0003 | allocate the point table on demand |
| Reach as a byte matrix (R11) | about 10 MB at 2,000 tests × 5,000 functions (mapped without reserving memory up front) | a sparse set per function |
| One `n + 1` change per integer literal, no statement deletion (R10) | some off-by-one directions and dead statements are not probed | more operators when survivors show they are needed |
| Mutant children one at a time (R11) | run time grows with the number of changes | a bounded pool of children, results reordered as tasks do |
| Full recompile in each child (R11) | milliseconds per change on large programs | recompile only the changed item once generic call sites are shown not to go stale |
