# Results: vo-lang v2 (002-effects-functions-generics)

Recorded 2026-11-13 on an x86_64 host. `bin/vo` runs the RV64 binary under `qemu-riscv64`.

**Summary.** Effects with scopes, the effects record, functions as values, generics and migration are implemented, and every quality gate passes. Every success criterion that does not need the agent benchmark is met. Benchmark v2 (60 tasks, harness, grading) is built and validated, but its run was stopped by request after a first attempt turned out to be invalid. So SC-001, SC-002, SC-003 and SC-006 are not measured.

## Build

- 31 assembly files, about 31,300 lines (v1: 23 files, about 21,200), one static binary.
- New files:
  - `types.s`: function types and generics.
  - `effects.s`: effect sets.
  - `effects_record.s`: `vo.effects`.
  - `check_effects.s`, `check_func.s`, `check_generic.s`: the checker parts.
  - `rt_effects.s`: run-time enforcement and the effect builtins.
  - `rt_func.s`: higher-order builtins.
- The SESI `riscv64` lint is clean for every file except `src/jmp.s`, which was already exempt in v1. Findings are in `sesi-linter-issues.md`; this spec added #14.

## Quality gate (`make test`): passing

| Check | Result |
|---|---|
| Golden cases (`tests/run.sh`) | 210 / 210 (v1: 134). New areas: `effects` (40 files), `func` (31), `generic` (27) |
| Determinism (`--repeat 3`: run, effects, func and generic cases) | 108 / 108 |
| Fuzz, quick (200 iterations) | 0 failures |
| `apply_fixes.py` | 65 / 65 fixes resolve their diagnostic (v1: 41) |
| `catalog.py` | 70 codes (v1: 52): every Example produces its code; every Fixed version checks and runs clean; every code has a golden case |
| `ref_examples.py` | 28 examples pass; the reference is about 8,160 tokens (budget 15,000) |
| `fmt_corpus.py` | 114 files: idempotent, behavior unchanged |
| `migrate.py` (SC-005) | 61 / 61 |
| `record_changes.py` (SC-004) | 24 change sets, 21 of which change effects: 0 problems |
| `scope_fuzz.py --iterations 300` (SC-003a) | 300 paths (100 inside, 200 outside), 30 env names: 0 mismatches |

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: benchmark v2 not at the ceiling | **not measured** (see Benchmark v2) |
| SC-002: vo solves at least as many tasks as Python, with ≥20% fewer attempts or ≥10 points more solved | **not measured** |
| SC-003: 100% of vo solutions exceeding their allowance are rejected; Python's undetected rate recorded | **mechanism verified, not measured on agent runs**. In all 10 capability tasks, the deliberately violating reference solutions are caught in both languages: vo by its declaration (or R0010), Python by the sandbox (`bench2/validate.py`). No agent run has produced a violation yet. |
| SC-003a: randomized paths and env names: 100% of out-of-scope accesses refused, no in-scope access refused | **met**: 0 mismatches over 300 paths (`..`, absolute paths, symbolic links in and out of the scope) and 30 computed env names |
| SC-004: for 20 effect-changing change sets, the record diff lists exactly the changed functions | **met**: 21 effect-changing sets plus 3 no-change sets. Exact lists, no false positives or negatives; `check` and `--update` agree; the record lines that change are exactly the listed functions |
| SC-005: 100% of v1 programs migrate in one round of first fixes, byte-identical output | **met**: all 61 golden programs and benchmark solutions. Only E0501 (plus diagnostics v1 already reported) appears; one round of fixes checks clean, is canonical, and produces identical stdout and exit status |
| SC-006: feature set: ≥90% within three attempts, ≥75% on the first | **not measured** (1 of 20 valid results so far) |
| SC-007: reference ≤ 15,000 tokens | **met**: about 8,160 tokens, with a runnable example in every section |
| SC-008: check + run of a 1,000-line program using effects, closures and generics < 1 s | **met**: 1,058 lines in 95 ms (check 63 ms, run 32 ms); the v1 program takes 87 ms |
| SC-009: fuzzing the new syntax: no crash, hang or unstructured output | **met**: 3,000 iterations (seed 9) with v2 syntax fragments added to the mutations, 0 failures |

## Benchmark v2

### What exists

- **Tasks:** `bench2/tasks/`, 40 tasks. Each task has a statement, visible and hidden cases, and reference solutions in both languages; each refactor task also has starter trees in both languages.
  - multi_file `m01–m10`: 4–5 files per reference, 10–14 hidden cases each.
  - refactor `r01–r10`: starters with 3–6 files and 150+ lines, in both languages; each starter fails exactly the hidden cases that test the change.
  - hidden_property `h01–h10`: 15–21 hidden cases each; naive solutions fail some of them.
  - capability `c01–c10`: allowances in `uses` syntax, plus violating variants.
- **Feature set:** `bench2/features/`, 20 vo tasks.
- `bench2/validate.py`: 40 tasks and 20 features, 0 problems.
- **Harness** (`agent.py`, `grade.py`, `py_sandbox.py`, `summarize.py`):
  - Multi-file replies; three attempts.
  - Feedback shows only the visible cases and a count of failed hidden checks.
  - Both languages get the same 30 s per case and a clean environment.
  - Capability grading: vo by its declaration and observed behavior; Python by observed behavior only.

### The first run was invalid

The first full run (archived in `bench2/results/invalid-tools-enabled/`, not used here) called `claude -p` with its defaults. That caused three problems:
- **The agent could read the answers.** It had read-only tools that need no permission in headless mode, and it ran from the repository root, so it could read the reference solutions and hidden cases.
- **It wasted time on refused tool calls.**
- **The user's own hooks and plugins were loaded into every session.**

The harness now runs every attempt as `claude -p --tools "" --setting-sources "" --strict-mcp-config --no-session-persistence` from an empty directory. The v1 benchmark's results (`bench/results`) were collected the same flawed way and should be re-run before they are relied on.

### Partial valid results

The corrected run was stopped by request. It got this far:

| Run | Tasks | Solved on first attempt | Hidden checks | Violations |
|---|---|---|---|---|
| vo, main tasks | 1 / 40 (capability) | 1 | 100% | 0 |
| Python, main tasks | 7 / 40 (capability) | 7 | 100% | 0 |
| vo, feature set | 1 / 20 | 1 | 100% | — |

That is too little to judge SC-001, SC-002, SC-003 or SC-006.

### Cost

- **vo calls are slow.** A vo agent call takes minutes where a Python call takes 5–20 s. The vo prompt starts with the 8,000-token reference, and the model writes in a language it knows only from that page.
- **Grading is not the problem:** 0.5–2 s per task for both languages.
- **Time estimate:** a complete run needs roughly an hour or more per vo set.

**To resume:** `agent.py` appends one record per task, and `summarize.py` keeps the latest record per task. Run only the missing tasks with `bench2/run_agent.sh --lang vo|python [--set features] --only ID,...`.

## Bugs found and fixed during this work

| Bug | Found by | Fix |
|---|---|---|
| `interp_reset` and `run_test_body` were in `.rodata` (from v1). This only worked while string lengths happened to keep the code 2-byte aligned; v2's strings broke it and every `vo test` crashed | Crash bisection | Added the missing `.text`; logged as `sesi-linter-issues.md` #14 (the lint did not report code in a data section) |
| A map `remove` followed by inserts could hang forever (from v1): `map_grow` could pick a capacity that is not a power of two, while probing masks with `cap*2-1` | Benchmark task author | Capacity now always doubles; golden case `run/map_remove_grow` |
| Effect parameters were lost at run time when a callback was forwarded to another `E: effects` function, or to the function itself recursively (R0010 on a correct program) | Feature-set author | `es_subst` now accepts the run-time "any owner" marker; golden case `generic/effect_forwarding` |
| The reference's "Types" section said there were no function types or generics | Feature-set author | Text corrected |
| `x.f()` could not be told apart from v1's "no methods" error | Design | The parser keeps the rewrite; the checker decides (a field of function type, or E0110); with parse errors the parser reports it as in v1 |
| A function type's `uses` clause inside a parameter list swallowed the next parameter | Catalog example | A comma continues the clause only if the next name is not followed by `:` |

## Deviations from the plan and contracts

- **More source files than planned:** checking, the record and the runtime have files of their own (listed under Build).
- **AST layout:**
  - `N_LAMBDA` has the same layout as `N_FN`, with a made-up name node ("anonymous function"), so existing function code handles both.
  - Function types are `N_TYPE_NODE` with `TY_FUNC` rather than a separate `N_FN_TYPE`.
  - `N_FN`'s side record hangs off its name node.
- **Effect sets have a second mask, `ES_ANY`,** for effects a builtin needs "in any scope". These can overlap the declared scopes, and none of them counts as unused. **Effect-parameter bindings travel as `TY_EFFSET` types**, so they can sit among type arguments.
- **Codes added to the contract:** E0509 `unknown_effect` and E0510 `invalid_effects_record`. **R0011 was dropped.** E0502 and E0508 were merged into E0502 `function_value_effects`.
- **W0504 is reported only for multi-file programs;** a warning on every single-file check would be noise.
- **The return type of an anonymous function is always written** (constitution I: one spelling). The spec's FR-017 was amended.
- **Builtins are not function values;** E0201 says to wrap them. The reference documents this.
- **A function returning a function type writes the type's `uses` first:** in `fn f() -> fn(str) uses console {`, the clause belongs to the returned type.
- **E0310's annotation fix** looks at the next statements of the same block for a call that passes the variable to a generic function. Other later uses do not produce a fix.
- **The SC-005 corpus** (`tests/migrate/v1/`) holds golden programs and benchmark solutions. Docs examples are checked by `catalog.py` and `ref_examples.py` instead.

## Known limits and follow-ups

1. **Finish benchmark v2** with the corrected harness (SC-001, SC-002, SC-003, SC-006), and re-run v1 for comparison.
2. **`vo run --timeout` is checked only in vo loops,** not inside run-time routines. Nothing in the runtime loops unboundedly now that the map bug is fixed, but a deadline check in long runtime operations would make it robust.
3. **Benchmark task gaps:**
   - c03 and c08 never set their environment variable, so only the default path is tested. The grader would need per-case environments.
   - c05 depends on cases running in order in one directory.
4. **The Python sandbox** uses audit hooks and wrappers, so it misses effects done through C extensions or `ctypes`. The upgrade path is seccomp or strace.
5. **`Pair[fn() uses console, Foo]`:** a user type name after a function type's effects is read as an effect parameter. This is rare, and the parameter-list case is handled.
6. **Generic runtime operations** (equality, rendering and ordering on type parameters) substitute types on each call. This is marked with a `ponytail:` comment; specialize hot paths if generic code turns out to be slow.
