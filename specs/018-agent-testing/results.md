# Results: Agent-Friendly Testing

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/test_scale.py`: 200 tests and properties in 20 files listed in 0.02 s (limit 1 s); every one of the 200 listed identifiers, passed to `--test`, runs exactly that test. |
| SC-002 | met | Every cause has a golden case (`tests/cases/test/cause_*`, 12 cases: the 11 causes plus an error inside a task). `tests/json_stream.py` (part of `make test`): every stdout line of the 45 golden outputs of `vo test --json` parses as JSON, and every non-passing test record in all golden and step expectations (191 step outputs) carries a documented cause. |
| SC-003 | met | `tests/cases/test/cmp_*`: each operator on int, float and str, a list, a literal side, `and` (with short-circuit), `not`, `or`, long values. `tests/cases/test/diff_*`: index, length, case, payload, map key (missing and different), result, ordered map, deque, long string (character position) and a scalar. Two 10,000-element lists of records differing in one field report `first difference at [412].name`. |
| SC-004 | met | `tests/cases/coverage/kinds` lists exactly its seven gaps (function, else, if_false, case, loop, two lambdas) and one then-branch, no more; `tasks`, `function_values`, `selection`, `failing_test`, `no_tests`, `hole_function`, and the step case `coverage_std_pkg` (standard library and package excluded). Overhead with `--coverage`: 1.11x on a compute-heavy test (0.403 s without, 0.449 s with; limit 1.5x). |
| SC-005 | met | `tests/mutate_eval.py`: 6 fixtures (boundary, loop end, sign, `and`/`or`, negation, constant); in each the known change survives the weak tests and is caught once the missing test is added. `--mutate` on every example with tests: expenses 0.1 s (23 changes), helpdesk 0.0 s (17), notes-api 0.2 s (65), release-check 0.0 s (9), status-board 0.0 s (15), url-check 0.0 s (0); limit 60 s. |
| SC-006 | met | Reference: about 12,234 tokens, +547 (limit +700, budget 15,000). The full gate passes on x86-64, and `make test` on ARM64 and RISC-V (Verification). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 674, step 210 + 191 + 35 repeated, catalog 109 sections, reference 37 examples, site 44 pages / 191 examples, doc conformance, `json_stream.py`, migration 61/61, fuzzers, examples, diff-engines 1,000 (0 disagreements) |
| `make gate` (x86-64) | passed: lint; test; ubtrap; diff-asm; the tasks and files-and-processes determinism steps; agent testing determinism (new: 20 runs of 101 golden and 2 step cases, all identical); agent testing scale (`test_scale.py`, `mutate_eval.py`); gc-check (stress golden 674, stress steps 191, 703 bench2 reference runs unchanged, every injected fault caught, stress diff-engines 1,000: 0 disagreements); speed geometric mean 0.883 (limit 1.10) |
| `make instrumented` | 674 of 674 under the instrumented build |
| `make security-check` | passed: the spec 013 and 017 races, package race and server attacks unchanged (0 failed) |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): golden 674, step 210 + 191 + 35, catalog, reference, site, doc conformance on both |
| `tests/test_scale.py` | as SC-001 and SC-004 |
| `tests/mutate_eval.py` | as SC-005 |

## Behavior changes

- **JSON test records** gain `id` (`FILE::NAME`) on every record, and `cause` on every record that is
  not `pass`; the test summary gains `selected`. Every golden and step case with a JSON test record was
  regenerated; the differences were checked by script to be only these fields (and the ones below).
- **`expect` on a comparison** says which comparison failed and shows its operands
  (`` `expect` failed: `n > 3` is false. `` and `    n: 2`); `expect(a or b)` names both parts. The old
  message remains for any other condition. Golden cases and site pages with the old message
  (`prop/negative`, `tasks/in_test`, the properties guide, and others) were updated.
- **`expect_eq`** adds `first difference at PATH: ...` for non-scalar values and long strings
  (`generic/generic_eq_render_test`, `test/values` and others).
- **Captured output**: a passing test's `print` output no longer appears in the text report
  (`prop/uses_console`); a failing test's appears under it, indented; with `--json` it is the `output`
  field, so the stream is JSON only.
- **New warnings** W0602 and W0603 appear in existing fixtures whose tests compare literals or check
  nothing (26 golden cases: `spec/*`, `prop/*`, `test/values`, `test/dir_run` and others); warnings never
  change an exit status. The `holes` count of `vo check --json` now counts W0601 only (it counted every
  W06 code).
- **Usage messages**: the unknown-option list names the new options (`cli/bad_flag`); `--diff` "only
  works with `impact` or `test`" (`query/usage_diff_run`).
- **New codes** W0602, W0603, T0011, T0012 (contracts/codes.md); docs/errors.md, the `vo test` tool page
  and the tests guide describe them.

## Deviations

- **W0602 does not count a test that calls a function with `ensures`**: such a property is the pattern
  the contracts guide teaches (the contract checks every result). Found when the guide's examples began
  to warn.
- **Mutation candidates come from the compiler**, which visits every expression of the program's own
  functions (outside contracts), instead of a separate AST walk; the changes are then sorted by
  position. Changes are type-preserving by construction (the operator pairs of research R10).
- **The `id` field is written in place** (no string allocated during a test's report), so `--mem-stats`
  figures did not move.
- **The `--diff` cases** reuse the `vo impact` fixture program (`tests/cases/query/impact/test_diff*`,
  `test_mutate_diff`) instead of new step cases in `tests/cases/test_steps/`.
- **Operands of an `expect` inside a task** (a lambda run by `concurrent_map` in a test) are not
  forwarded from the task: the message names the comparison, without the values.
  `ponytail:` forward them through the task's result pipe if needed.
- **Cause in text**: shown in JSON only (FR-017, amended in analysis).
- **Mutation testing tables** live in permanent memory: each test starts with a fresh heap, which would
  otherwise free them between tests (found by the first `--mutate` run, which crashed its children).
  The collector stress suite (`make gate`, gc-check) found three more objects left unrooted on the
  collected heap: the source text of a survivor while its changed text is built, the sort arrays of
  the coverage report, and a map's key list in the first-difference walk. It also found that mutant
  children recompiled with the collector on. All four were fixed, and the new cases pass under the
  stress build.
