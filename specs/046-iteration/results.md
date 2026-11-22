# Results: Iteration (spec 046)

Status: implemented. `make test` passed (one run). The gate was not run.

A second variable in `for`, one new instruction (`OPC_FOR_VALUE`), one new warning (W0403), one
condition in the checker for `sort_by`, and `std/lists.vo` written in vo.

## SC-001: the loops against a model

`python3 tests/iteration.py --group loops`: 1,000 generated programs (348 over a map, 163 over an
ordered map, 327 over a list, 162 over a deque) with bodies that print, update, add, remove, remove
and put back, push, pop, change a later element, `break`, `continue` and loop again inside, each
print what the Python model of contracts/iteration.md says. On ARM64 and RISC-V under qemu: 100
programs each (not 1,000), all agreeing.

Two deliberate faults were put in to see that the programs notice: a removed key ending the loop
instead of being skipped (31 programs differed), and a list loop reading the element before the
index (379 differed). Both were removed.

Golden cases: `run/for_pair_list`, `run/for_pair_map`, `check/for_pair_errors`,
`check/for_pair_syntax`, `fmt/for_pair`.

## SC-002: the cost of a map loop

A map of a million entries, summed (best of 5, x86-64): `for k in keys(m)` with `m[k]` 132 ms,
`for k, v in m` 123 ms. The smallest `--max-memory` (in steps of 8 MiB) under which the old form
completes is 48; the new form completes under it. Emulated, times only: 388 and 411 ms (ARM64), 492
and 427 ms (RISC-V).

## SC-003: the code moved

- `std/`: six loops over `0..len(xs)` that began with `let x = xs[i];` are two-variable loops now
  (three in `std/web.vo`, three in `std/security.vo`); all six were rewritten by hand, three of them
  over a field (`p.ways`, `p.rules`, `app.routes`), which the warning does not match. `has_item` in
  `std/web.vo` is replaced by `std.lists.contains`.
- `std/` and `examples/` had no loop over `keys(m)` that read `m[k]` first: the 13 counted in the
  spec were all in `tests/cases/`.
- `vo check` reports no W0403 in any example or in the library (the catalog check failed on two
  entries whose examples import `std.web` until the loops there were moved).
- Left as they are: 23 loops over `0..len(xs)` and 5 over `keys(m)` in `std/` and `examples/` that
  do not begin with the read (they need only the index or the key, or read something else).
- Golden outputs changed: `web/errors.err` and `web/steps.err` (line numbers of `std/web.vo`, one
  import line more). No recording fingerprint changed.

## SC-004: `std.lists`

`--group lists`: 10,200 calls (2,000 each for `contains` with `index_of`, `reversed`, `sort_by_desc`
with int, text and float keys, `min_by` with `max_by`, `group_by`; 200 for `sum`) agree with Python,
and no call changes its argument except `sort_by_desc`. Golden cases `run/lists`,
`run/lists_sum_overflow` (R0001), `check/sort_by_param_key`.

`sort_by_desc` is the builtin `sort_by` between two reversals in place; 200,000 ints: 26 ms.

## SC-005: tools

- `python3 tests/grammars.py`: 7 checks, none skipped: the regenerated tree-sitter parser, the
  highlight tests (one new assertion), 1,520 files parsed, TextMate agreement, Vim.
- `query/pair_first`, `pair_second`, `pair_index` (the type at each variable, which is what hover
  shows), `query/pair_refs`, `edit/rename_pair` (both variables renamed, then checked and run),
  `coverage/for_pair`, `mutate/for_pair` (3 changes, 3 caught).

## SC-006: budget and suite

- Reference: 14,979 tokens of 15,000. To fit, the `std.security` row lost five names.
- `tests/doc_conformance.py`, `tests/catalog.py` (W0403 added; `specs/046-iteration/contracts/codes.md`
  is in its list), `tests/apply_fixes.py`, `tests/site_examples.py` and `make site-check` pass.
- `run` (120) and `check` (69) pass on ARM64 and RISC-V under qemu.

## Deviations

- **A two-variable loop over a range is reported by the parser** (E0301 with the fix), which then
  reads the loop as the one-variable loop over the range. Parse errors stop the check, so the case
  is `check/for_pair_syntax`, separate from the other errors.
- **The E0301 fix is missing when the first variable is `_`** (`for _, c in "abc"`): the node keeps
  no position for `_`.
- **A two-variable loop over a wrong type also gets "never used" warnings** for variables its body
  does use only through erroneous code; the one-variable loop does the same.
- **A loop variable named like a builtin** (`for len, x in xs`) is accepted, as `for len in xs` is
  today; the spec's scenario expected an existing error and there is none.
- **Hover** is tested through `vo query type-at`, which `tests/lsp.py` already compares hover with;
  no hover request was added to `tests/lsp.py`.
- **`@allow` does not take W0403** (it takes W0701 to W0705 only), so the warning cannot be silenced
  where the old form is wanted; the speed test's program simply carries the warning.
- **SC-001 on the emulated architectures** ran 100 programs, not 1,000.
- **The guide**: `holes.md` keeps `for k in keys(sums)` (those loops need only the key until the hole
  is filled, and their outputs show positions); the other pages use the new loops.
- T003's parse case is `check/for_pair_syntax`; there is no separate case in `tests/cases/parse`.

## Not verified

- The gate.
- Real ARM64 or RISC-V hardware (qemu only).
- The grammars in a real editor.
- Whether W0403 fires in programs outside this repository in a way their authors mind: it is given
  only for a `let` or a parameter and the exact first statement.

## Noticed, not changed

- The reference has 21 tokens left.
- `examples/README.md` still describes notes-api as built on `std.http.serve`.
