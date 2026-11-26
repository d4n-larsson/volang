# Results: Anonymous Functions Take Their Types From Context

## Setup

- T003 baseline (commit `c57c144`, x86-64), counted by `tests/lambda_stats.py` (an anonymous function is
  `fn(` after `(`, `,`, `=`, `[`, `{` or `return`; vo files and the vo code blocks of Markdown files;
  std's doc-test comment lines included):

  | Path | Lambdas | With written types | Header characters | Single-return | Their lines | All lambda lines |
  |---|---|---|---|---|---|---|
  | `examples` | 22 | 22 | 460 | 21 | 63 | 69 |
  | `std` | 72 | 72 | 2,433 | 57 | 77 | 251 |
  | `packages/postgres` | 2 | 2 | 41 | 2 | 4 | 4 |
  | `docs/reference.md` | 6 | 6 | 117 | 5 | 15 | 18 |

  (Research R10's 103 for std came from a looser grep that also counted function types; the script's
  rule is the one SC-001 and SC-003 are measured with.)
- Reference: ~15,000 of 15,000 tokens (`tests/ref_examples.py`).
- `vo check`, best of 5: `examples/notes-api/main.vo` 20 ms, `examples/secure-notes/main.vo` 22 ms, the
  8,000-item program of `tests/perf.sh` 165 ms.

## SC-001

Pending.

## SC-002

Pending.

## SC-003

Pending.

## SC-004

Pending.

## SC-005

Pending.

## SC-006

Pending.

## Verification

Pending.

## Deviations

- The spec's edge case said a `_` parameter is "allowed ..., as today"; today it is not allowed
  (reference, Lexical rules). Kept as today; spec corrected (T004).
- `tests/cases/parse/lambda_errors` tested that a lambda parameter needs a type (E0110); the spec removes
  that rule, so its expected output changed in T010 (no error); T019 adds E0342 to it.
- E0343 is not reported for an anonymous function without parameters (`let g = fn() => 41;`): nothing
  is unknown, its return type comes from its body. The spec's "no expected type" edge case is read as
  "parameter types unknown" (T017).
- E0413 and `vo fmt`'s rewrite to `=> expr` apply to untyped anonymous functions only: a 0.4 lambda gets
  E0342 first, and E0413 in the next round of fixes once its types are gone. Firing E0413 on 0.4 code would
  have stopped every existing program with a single-return lambda from checking before the migration.
