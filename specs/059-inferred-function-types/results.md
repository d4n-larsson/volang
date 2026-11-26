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

**Partly met.** No anonymous function in `std/` or `examples/` writes a type any more (93 before,
counted again at `c57c144` with the corrected `tests/lambda_stats.py`: 71 in std, 22 in examples; 91 now,
0 with written types). Every one migrated by fixes except those nothing gives types to, edited by hand.
That share is far above the target of 5%: 33 of 93 (about 35%) needed a typed binding or a named
function: 15 typed bindings in std code (five route handlers of `std.web` and their health and metrics
routes, two `serve_workers` closures and a worker body, whose input or message type appears only in the
function's own parameters; five untyped `let`s, which E0342's fix moved onto the binding), 16 in std's
doc examples (the `on_*` routes, `typed`, the auth steps, `serve_workers`, `serve_app` and `transaction`,
where `fn() => ok(...)` leaves the result type open), and 2 named setup functions in the examples. The
spec assumed most lambdas sit where the call gives them their types; std's web and serving layer is the
exception, and that is most of std's lambdas. User code in `examples/` needed 2 of 22.

## SC-002

Met on x86-64 (ARM64 and RISC-V: see Verification). The migrated golden cases compared with their
expected output from before the change (`tests/migrate_059.py compare`): 112 of 123 equal after
normalizing positions and fix text; the 11 others were read one by one and kept their meaning (hand
edits where no fix applies; positions, recording fingerprints, std line numbers in traces and a new local
`serve_one` in a frame). `tests/gc/baseline.py`: 731 of 731 bench2 reference runs unchanged;
`bench2/validate.py`: 40 tasks and 24 features, 0 problems; `tests/diff_engines.py`: 1,000 programs, 0
disagreements (the oracle runs the 0.4 text, the current build the migrated text).

## SC-003

Partly met. Characters of anonymous function headers in `std/` and `examples/`: 2,675 → 690 (−74%,
target −50%). Lines of single-return lambdas: 140 → 79 (−44%, target −60%): in `examples/` 63 → 19
(−70%), in `std/` 77 → 60 (−22%), because std's single-return lambdas were mostly in one-line doc
examples already.

## SC-004

Met. E0342, E0343 and E0413 each have golden cases (`tests/cases/lambda_types/`) and catalog entries
with a broken and a fixed example; `tests/catalog.py` 164 sections, 0 problems; `tests/apply_fixes.py`
266 of 266 fixes resolve their diagnostic.

## SC-005

Met. The reference was ~15,000 tokens before and is ~14,999 after (`tests/ref_examples.py`, 40 examples
pass): the rule's paragraph is paid for by its lambda examples, now `=> expr`, and two shorter phrases.

## SC-006

Met. `vo check`, best of 5 (baseline in Setup): `examples/notes-api/main.vo` 17 ms (20),
`examples/secure-notes/main.vo` 20 ms (22), the 8,000-item program 156 ms (165).

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
- The spec's order "non-function arguments first" became "arguments that are not anonymous functions
  first" (plan research R3). An anonymous function without parameters keeps its place in a call: it
  needs nothing from the others, and moving it changed which argument binds an effect parameter
  (`security/c2_bound_param`).
- E0342's fix that removes written types is offered when the call leaves the return type open only for a
  return type no body needs context for (a scalar, `void`, a non-generic record or variant): a body
  `return ok(x);` loses its type without one (E0307). Such lambdas were edited by hand.
- `try` gives a call with an anonymous function argument its expected type
  (`let n: int = try transaction(db, fn() { ... })`); found while migrating the guide.
- Old engines (the oracle, the assembly release) read only the 0.4 form: `tests/diff_engines.py` and
  `tests/diff_asm.py` give them the generated program and the current build its migrated text, comparing
  output without positions when the text changed; `diff_asm.py`'s mutants leave out corpus files in the
  0.5 form.
- `tests/migrate_056.py` keeps its claim for programs written before 056 with three hand edits it applies
  after the fixes (`HAND_059`: two setup functions named, one `=> e` on a generic return), and pins the
  package again between rounds of fixes.
