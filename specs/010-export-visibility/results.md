# Results: Visibility with `export`

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `multi/not_exported(_json)`, `not_exported_twice`, `private_type`: E0209/E0210 with the fix; `tests/apply_fixes.py`: 71/71 fixes resolve their diagnostic, including the new ones. |
| SC-002 | met | Importing every `std` item: 48 exported items check, 33 internal helpers are E0209 with no fix (script over `std/*.vo`); `check/std_internal`; `tests/doc_conformance.py` checks exported = documented. |
| SC-003 | met | 743 entry programs in `bench2/`, `tests/`, `examples/` checked: no E0209/E0210 except the six cases that test them; `make test` and every evaluation suite pass on the migrated programs with byte-identical outputs (changed golden outputs differ only by the migration, see US3). |
| SC-004 | met | 3 isolated agent runs (`claude -p`, no tools, empty directory, the reference only) on a two-file task with a private helper: 3/3 wrote `export fn mean` with a private `total` on the first try; each checks, prints 5, and importing `total` is E0209. |
| SC-005 | met | Reference ~10,517 → ~10,576 tokens (+59, limit 250); 34 examples and 95 catalog sections pass. |
| SC-006 | met | Final gate below: `make gate` passes; ARM64, RISC-V and the instrumented build pass; 10,000 generated programs give 0 disagreements with the assembly release. |

## US1: a file keeps its helpers to itself

- `export` is token 47; `parse_item` sets `NF_EXPORT` (512) and starts the item's span at `export`; `vo fmt`
  prints `export ` (`fmt/export`).
- E0209 in `bind_imports` (`src/loader.c`), E0210 over the exported items' type nodes (`src/check.c`). One
  diagnostic per declaration carries the fix; further imports or uses are its related notes ("Also needed
  here."), so applying every first fix never writes `export export`.
- Cases: `multi/export_ok`, `not_exported`, `not_exported_json`, `not_exported_twice`, `private_type`,
  `parse/export_misplaced`, `export_misplaced_block`, `export_twice`, `export_as_name`.

## US2: the standard library shows only its interface

- `export` on the 48 documented `std` items; the 33 helpers are private. `vo doc` lists exported items
  (`src/doc.c`); its output for `std` is byte-identical. An exported package item without a doc comment is
  listed with no description line.
- Package items: E0209 says "internal to package `NAME`" with no fix (`packages/private_item`, which also
  shows `vo doc` listing only the exported items).

## US3: existing programs move to the rule

- Migration (one-off script, the same edit as the fix): `bench2/` 413 declarations in 102 files (the E0210
  pass exported 22 types in 17 programs), `tests/` 413, `examples/` 13, the docs' examples 24 (and their
  package pins), `std/` 48. `git diff` of the migrated sources shows only `export ` added at the start of
  declaration lines.
- Golden outputs re-accepted only where the diff was the migration: spans on a migrated line 7 columns
  later, the new text of that line, package hashes, recording fingerprints. Commands that address a name by
  `FILE:LINE:COL` on a migrated line moved 7 columns (`query/*.cmd`, `edit/rename_param_pos`); the stored
  diffs of `query/impact` gained `export` on the same lines.
- Step cases whose `write` steps replace package files (`packages/json`, `package_effects`, `update`) write
  the exported version; the tamper cases stop at the hash check and keep their sources.
- The evaluation suites import across files the migration script did not see: `tests/prop_eval/*/props.vo`
  imports the solution (exported in the task's reference copy and in every mutant of that file, 92
  declarations), `tests/stdlib_eval/t10-money-package`'s package exports `cents`, and the rename targets
  and expected references of `tests/refactor_eval/*/` moved 7 columns on migrated lines; the
  `effect_changes` `after/` overlays export what `before/` exports or imports.
- `tests/effect_changes/17-import-effectful`: the change it tests is "start importing an effectful
  function", so `stamp` is now exported in `before/`.

## US4: tools and editors

- `vo query impact` recognizes a removed `export fn NAME(` line as a removed function
  (`query/impact/removed_fn` covers it).
- `vo edit rename` and `vo query refs` needed no change (they work on names): `edit/rename_fn_imported`
  renames an exported function and keeps `export`; `query/refs_fn` lists an exported declaration.
- IntelliJ plugin 0.1.4: `export` is a keyword (lexer test); `../volang-intellij-plugin/build/distributions/volang-intellij-plugin-0.1.4.zip`.

## Final gate

| Check | Result |
|---|---|
| `make gate` (x86-64) | passed: 478 golden cases, 199 + 113 step cases, ubtrap, diff-asm 2,000 (0 disagreements), gc-check, speed geometric mean 0.917 (limit 1.10) |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | passed |
| `make instrumented` | passed |
| `make diff-asm COUNT=10000` | 0 disagreements |
| `make stdlib-eval` | 11 of 11 tasks |
| `make refactor-eval` | passed (53 of 53 edits correct with `vo edit`) |
| `make prop-eval` | 61 of 62 mutants caught (98%); fails only on `h09-window-max/05.vo`, "not a valid mutant", which fails the same way on the commit before this spec (see Deviations) |
| reference | ~10,576 tokens (+59) |

## Deviations

- **`export` inside a block or a record** gives the existing message of that position ("Expected a
  statement, found `export`."), not one naming where `export` is allowed (spec Edge Cases); top level
  names it ("Expected `fn`, `record` or `variant` after `export`").
- **E0204 then E0209**: the E0204 fix (add the missing import) can lead to E0209 when the item is private,
  so that path takes two fix rounds.
- **`FileRec.pkg` was never set** (since spec 007); package files are recognized by `pkg_relpath` instead.
- **Recordings**: the program fingerprint covers every loaded file, `std` included, so a recording made
  before this change replays with the "program differs" note (output unchanged).
  `examples/status-board/recordings/demo.jsonl` was re-recorded; `http/replay_old_recording` keeps its old
  recording and shows the note.
- **`bench2/` changed once** (spec Assumptions): benchmark runs from before spec 010 used sources without
  `export`. `tests/speed.py --bench2` strips `export` for its oracle; that oracle build (`build-oracle`)
  is not present today.

- **`make prop-eval` was already failing**: `h09-window-max/05.vo` (a correct but slow O(n·k) window
  maximum) passes every hidden case, so it is not a valid mutant; the same run on the commit before this
  spec fails identically. Not caused by spec 010; not fixed here.

## Follow-ups

- Replace or drop the `h09-window-max/05.vo` mutant so `make prop-eval` passes again.

- The E0204 fix could add `export` too when the found item is private (one round instead of two); it needs
  a fix with edits in two files, which the diagnostic format (one edit per fix) does not have.
- The recording fingerprint could leave out `std` files, so library changes do not mark every recording
  as made by another program.
