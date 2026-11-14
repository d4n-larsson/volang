# Implementation Plan: Query, Structural Editing, Trace and Replay

**Branch**: `006-query-edit-trace` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-query-edit-trace/spec.md`

## Summary

Five toolchain features. None changes the language; all reuse what the checker and the VM already know.

**1. Queries** (research R1–R6):
- **Reference log:** while `query`, `impact` or `edit` checks a program, the checker logs every resolved name with its target key, role and enclosing item. The hook sits next to the existing `rename_use` calls, which already see every resolved use; fields and cases log at their `member_index` sites.
- **Commands:** `vo query callers|refs|type-at ENTRY TARGET` filter the log. `type-at` also records the innermost expression containing the position when `check_expr` writes its type.
- **Impact:** `vo impact ENTRY --diff FILE` parses a unified diff, checks that it matches the current files, maps changed lines to items, and walks the logged call and value references backwards to affected functions and tests.

**2. Structural edits** (R7–R10):
- **Changes:** `vo edit rename` turns every logged site of the target (plus its `vo.effects` line) into a change list; `vo edit add-field` inserts the field into the declaration and into every record literal.
- **Verification:** the changes are applied to in-memory copies (an overlay), the checker is reset and run a second time on the overlay, and the edit is accepted only if the errors are unchanged and the renamed target's references are exactly the edited sites. Then all files are written, or none.

**3. Traces** (R11, R12):
- With `--trace`, the compiler emits a trace-enter instruction at the start of each function and picks a second handler table whose return handlers write `return` records. Runs without `--trace` execute exactly today's code.
- Output is JSON lines through its own buffer, bounded by `--trace-limit`, and always ends with an `end` record written from the `vo_exit` hook.

**4. Values in runtime errors** (R13, R14):
- At a runtime error, each shown frame's parameters and in-scope locals are rebuilt from the AST at the frame's position (every local has its own slot) and rendered with a 200-byte cap.
- The cap is a new mode of `render_value`; function values render as their type.

**5. Record and replay** (R15–R17):
- The eight builtins that bring outside data into a program record their results with `--record`, and in `vo replay` return the recorded results in order. The seed and arguments go in the header.
- A mismatch is R0014; a changed program is noted and run.

**6. Evaluation** (R19): `tests/refactor_eval.py` applies at least 30 scripted edits to the r01–r10 starters with `vo edit` and with whole-word text replacement, and checks at least 30 queries against hand-written lists.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as. The existing v5 toolchain, extended in place.

**Primary Dependencies**:
- None at run time.
- Build: `binutils-riscv64-linux-gnu`.
- Running on x86: `qemu-user`.
- Lint: the SESI `riscv64` pack.
- Developer scripts: Python 3.

**Storage**: Files only. Traces and recordings are JSON-lines files written where the user asks; edits rewrite program files (temporary file, then rename).

**Testing**:
- `make test`: golden cases (new: `tests/cases/query/`, single-invocation cases through `.cmd` files), the new multi-step harness `tests/steps.py` (edits, traces, record/replay: a case is a folder under `tests/cases/edit|trace|replay/` with a `steps` file and expected files; `run.sh` skips such folders), repeats, fuzzers, reference examples, catalog coverage, `diff_engines.py`.
- `make gc-check`, `make speed-check`.
- New: `make baseline` (spec 005 binary, `99b5d3f`) and `tests/speed.py --baseline build-base/vo` for FR-016/SC-006.
- New: `tests/refactor_eval.py` for SC-001/SC-002.

**Target Platform**: Linux user space, riscv64, a static ELF with no libc. Developed under `qemu-riscv64`.

**Project Type**: Compiler and runtime for a programming language, as a CLI.

**Performance Goals**:
- Runs without `--trace`/`--record` stay within 5% of the spec 005 binary on every speed-benchmark program (same method as spec 005).
- A query or edit on the largest refactor starter (~600 lines over 4–6 files) finishes in under 1 s under qemu (SC-007). An edit checks the program twice.

**Constraints**:
- Determinism (Principle III): query and edit output ordered by file load order and position; traces carry no timings; replay is exact for an unchanged program.
- The reference grows by at most 2,000 tokens (SC-008; about 9,144 now).
- Existing output is byte-identical except the frame values added to runtime errors (FR-025).
- No code is inserted into `vm.s`'s hot handlers; the spec 005 page pins stay valid.

**Scale/Scope**:
- About 4,500–5,500 new lines of assembly: `src/query.s` (~1,400: log queries, targets, type-at, diff and impact), `src/edit.s` (~1,100: rename, add-field, overlay check, writing), `src/trace.s` (~900: trace handlers and records, frame values), `src/replay.s` (~1,000: recording, JSON-lines reader, replay), and ~800 across the checker, loader, compiler, `rt_render.s`, `interp.s`, `rt_io.s`/`rt_effects.s`, `sys.s` and `main.s`.
- About 80 golden cases and step cases.
- 10 refactor scripts (≥30 edits) and 10 reference lists (≥30 names).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No language change. Each command has one form: the entry file always comes first, and targets have exactly four forms (`name`, `Record.field`, `Variant.case`, `FILE:LINE:COL`). The roadmap's `type-at main.vo:12:5` becomes `type-at main.vo main.vo:12:5`, so no command has a shorthand (R3). |
| II. Machine-readable feedback | **PASS.**<br>- New codes T0004–T0009 and R0014 (contracts/codes.md); the catalog only grows.<br>- New JSON kinds (`ref`, `type_at`, `impact`, `edit`, `replay`, trace and recording records) and added fields (`locals` on runtime-error trace entries, `trace` on a test `failure`); nothing removed or renamed, so schema 1 stays.<br>- Edit changes use the existing fix shape.<br>- A refused edit reports every diagnostic it would introduce, with spans in the current text. |
| III. Determinism | **PASS.** Query, impact and edit output have a fixed order. Traces have no timings. A recording captures every input the program receives (the eight input builtins, plus the seed and arguments), so an unchanged program replays byte-identically. A trace or recording interrupted by a signal is incomplete, and replaying it is T0008, not a guess. |
| IV. Test-first through the CLI | **PASS.** Every story starts with golden cases that fail on the spec 005 binary. All of them live under `tests/cases/`: single-command cases as before, and multi-step cases as folders with a `steps` file, which `catalog.py` also reads. Every new code gets a case. Reference examples for the new commands run in `ref_examples.py` (a new `vo-steps` example kind, run through `tests/steps.py`). |
| V. Single source of truth, minimal surface | **PASS.**<br>- `docs/reference.md` and `docs/errors.md` change in the same change; growth stated and capped at 2,000 tokens.<br>- Options added only where the spec asks: `--trace`, `--trace-limit`, `--record`, `--diff`, `--dry-run`.<br>- Only the two edits the roadmap names.<br>- `ponytail:` markers: call-based impact (no type-level impact), diff paths matched by suffix, the 200-byte render cap, the fixed trace limit default, `check_reset` as the way to re-check (a fresh process would be cleaner but is not available under qemu). |
| Implementation constraints | **PASS.** All shipped code is assembly with prototype comments and passes SESI lint. Python appears only in `tests/steps.py` and `tests/refactor_eval.py`. |

No violations. One design choice is recorded in Complexity Tracking because it adds a mechanism the toolchain did not have.

**Post-design re-check:** still passes. The trace design (R11) keeps runs without `--trace` on identical code; the recording design (R15) touches only the eight input builtins; the overlay check (R7) reuses the whole checker rather than reimplementing its rules.

## Project Structure

### Documentation (this feature)

```text
specs/006-query-edit-trace/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R1–R19
├── data-model.md        # Phase 1: reference log, changes, overlay, trace, frame values, recording state
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── cli.md           # commands, options, targets, exit statuses
│   ├── reports.md       # ref, type_at, impact, edit, summaries; runtime-error locals; replay note
│   ├── trace.md         # trace file records
│   ├── recording.md     # recording format and replay rules
│   └── codes.md         # T0004–T0009, R0014
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── defs.inc        # RL_*/RR_*, CH_*, EV_*, FILE_GEN, OP_TRACE_ENTER, new T/R code numbers
├── main.s          # query/impact/edit/replay commands; --diff, --dry-run, --trace, --trace-limit, --record
├── check.s         # ck_item; logging at rename_use, declarations, arms, fields, cases; N_ARM type; check_reset
├── check_expr.s    # logging at idents, calls, builtins, field access, literals, cases; q_best for type-at
├── loader.s        # overlay map, file generations; import-name logging
├── diag.s          # pos_offset (line:col → offset); FILE_GEN in file_find
├── compile.s       # OP_TRACE_ENTER at function start and vm_ops_trace when tr_on
├── query.s         # NEW: targets, callers/refs/type-at, diff parser, impact
├── edit.s          # NEW: rename and add-field changes, overlay check, accept/refuse, all-or-none writing
├── trace.s         # NEW: tracing handlers and table, trace records and buffer, frame values for runtime errors
├── replay.s        # NEW: recording writer, JSON-lines reader, replay of the eight inputs, R0014
├── interp.s        # runtime-error report: frame values (text and JSON); rt_reporting; test-failure trace capture
├── rt_render.s     # render_cap; function values as their type
├── rt_io.s, rt_effects.s   # record/replay hooks in the eight input builtins
├── sys.s           # vo_exit hook: flush trace and recording
└── testrun.s       # `test` trace records; failure `trace` in reports

docs/reference.md, docs/errors.md

tests/
├── cases/query/                 # golden query/impact cases (.cmd files; multi-file programs in subdirectories)
├── cases/{edit,trace,replay}/   # multi-step cases: one folder each, with a steps file + expected files
├── steps.py                     # NEW: runs step cases (run.sh skips folders with a steps file)
├── refactor_eval/rNN-*/edits.txt, refs.txt
├── refactor_eval.py             # NEW: SC-001/SC-002
└── (run.sh, catalog.py, ref_examples.py pick the new cases up; existing runtime-error expectations regain frame values)
```

**Structure Decision**: the existing single-project layout, with four new source files, one per feature area that has no existing home. They link after the runtime (Makefile `RUNTIME` list unchanged), so the VM's page placement does not move.

## Complexity Tracking

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| A second check of the program in one process (overlay + `check_reset` + file generations), R7 | FR-009 requires the edit to be verified by the checker before anything is written | A second process (`vo check` on a temporary copy) needs fork/execve of the running binary, which is not reliable under `qemu-user`; static clash rules would duplicate the checker and miss fields, cases and generics. A golden self-test (re-checking an unchanged program gives identical diagnostics) guards the reset. |
