# Results: Scoped Effects Through Package Functions

## Baseline (before the change, 2026-11-23)

- Reference: `ref_examples: 40 examples, ~14999 tokens of 15000`.
- The eleven cases that name E0503 (`const/scope_target`, `effect_sets/use_narrow`, `effects/env_denied`,
  `effects/net_port_narrow`, `effects/net_scope`, `effects/scope_too_narrow`, `fs/literal_outside`,
  `fs/rename_literals`, `process/literal_outside`, `security/l1_literal_scope`, `tasks/scope`): read one by
  one, none is a scoped caller of an unscoped callee (they are literal targets, scoped callees, or a
  run-time denial), so none is expected to change.
- `tests/scope_paths.py --count 300 --seed 1` on the build before the change: 229 of 300 fail (E0503
  for a scoped caller of an unscoped callee), 71 pass.
- Effects records of the eight examples copied to `build/050-baseline/`.

## Changed cases

None. `tests/run.sh` over every case directory after the checker change: 1,130 existing cases pass
unchanged. No golden case had a scoped caller of an unscoped callee.

New: 31 golden cases in `tests/cases/effects/` and `tests/cases/tasks/path_scope`, and four step
cases (`effects/path_scope_link`, `effects/helper_record`, `replay/path_scope`,
`packages/scoped_connect`). All of them failed on the build before the change (E0503, or no
expectation yet for the unchanged diagnostics).

## SC-001: helpers and package functions check and run

Pass. `helper_scoped_net`, `_read`, `_write`, `_delete`, `_env`, `_serve` (the six effects),
`_record` (a record field), `_chain`, `_test`, and `packages/scoped_connect` (a package's
`connect(options)` under `net("127.0.0.1:1"), entropy`).

## SC-002: generated call paths

Pass. `python3 tests/scope_paths.py --count 2000 --seed 1`: 2000 programs, 0 failures (depth 1 to 6,
plain calls, anonymous functions, generic functions with function values; `net` and `file.read`).
No run reached a target outside `main`'s scopes.

The model had to be made more precise than the plan while implementing: a function's requirement
includes what its callee needs in turn through plain calls (the existing fixpoint), but not through
function values or anonymous functions, whose types hold only the callee's declaration. And literal
hosts are not compared with `net` scopes at check time (only file literals are); that was so before
and is unchanged.

## SC-003: existing cases

Pass: see Changed cases. `apply_fixes.py` 145/145, `catalog.py` 0 problems, `doc_conformance.py` 0
failures, `effect_sets_records.py` 0 problems, `packages_check.py` passes.

## SC-004: effects records

Pass. The `vo.effects` of the eight examples are byte-identical to the copies taken before the change,
and `vo effects main.vo` exits 0 in each. `effects/helper_record` shows that a helper going from
`file.read("data/")` to `file.read` is still reported (E0504) until approved.

## SC-005: speed

- Calls: `src/vm.c` is not changed; frames and the call paths are as before. `make speed` was not
  run.
- Scoped reads through the walk, 100,000 `read_file` calls under `file.read("a/")` in `main`, the
  reading function declared without scopes (best of 3, native x86-64):

  | Call depth | After |
  |---|---|
  | 1 | 0.431 s |
  | 50 | 0.440 s |
  | 1,000 | 1.312 s |

  The same workload at depth 1,000 on the build before the change (the reading function scoped,
  since the unscoped form did not check): 0.60 s.
- **Not met**: the criterion "at depth 1,000 at most twice depth 1" (ratio 3.04). Each scoped file
  operation walks the frame chain twice (the check and the open), about 4.4 ns a frame. At depth 50
  the cost is 2%. The walk has its `ponytail:` comment in `src/rt_effects.c`; the upgrade is a
  frame link to the nearest caller with a different scoped set, or a per-operation cache. Decision
  for the reviewer: accept the ceiling (and change the criterion), or build the upgrade.

## SC-006: reference

Pass. `ref_examples: 40 examples, ~14999 tokens of 15000, 0 problems` before and after: the two
sentences were reworded with the same number of words.

## SC-007: three architectures

The new golden cases (78 in `effects` with the existing ones, plus `tasks/path_scope`), the four step
cases and 100 generated programs pass on `build-aarch64/vo` and `build-riscv64/vo` under qemu. The
full suite was run on x86-64 only (see below).

`tests/diff_asm.py --count 2000 --seed 1`: 0 disagreements with the assembly release.

## SC-008: spec 049's probes

Pass: `effects/helper_scoped_net` (the helper around `connect`) and `packages/scoped_connect` (the
two-file package with `connect(options)`).

## `make test`

Pass on the second run (x86-64, 53 steps, exit 0). The first run stopped at `tests/stream.py`'s timing
case ("files/in under 10 s": 15.2 s) while other programs were loading the machine; that script and
every later step passed when run on their own, and the same build took 4.3 s for that workload when
the machine was quiet. The instrumented build and the gate were not run.

## Differences from tasks.md

- T005: the per-case `check --json` twins were not added; each run case fails with exit 65 if the
  program does not check, so the run cases cover it.
- T010: the link case is a step case (`effects/path_scope_link`), since golden cases start in an empty
  directory. File cases read files that do not exist: an `err` value shows the access was allowed.
- T011: the replay case records a run that ends in R0010 and replays it with the files removed.
- T017: `helper_literal_outside` uses a file literal; literal hosts are not checked at check time.
- T026: not needed; the function-value cases passed with the change of T008.
- T027: `make speed` not run (the call path is unchanged); the depth timing is above.

## Not part of this change

- `python3 tests/site_examples.py --coverage` reports `site/docs/guide/basics.md: mentions W0102
  without linking errors/W0102`. That page is not touched by this change.
- Literal hosts at `std.tcp.connect` are not checked against `net` scopes by `vo check`.
- A value of type `fn() uses file.read` is still refused where a parameter is written
  `fn() uses file.read("data/")` (E0502), and that diagnostic's fix still offers the plain effect in
  the type.
